/******************************************************************************
 *  Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *      * Neither the name of The Linux Foundation nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

package com.android.bluetooth.csip;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothDeviceGroup;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.SystemProperties;

import android.util.Log;

import com.android.bluetooth.btservice.AdapterService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.HashSet;
import java.util.List;
import java.util.UUID;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import javax.crypto.spec.IvParameterSpec;

/**
 * Class that handles Bluetooth LE Scan results for Set member discovery.
 * It performs scan result resolution for set identification.
 *
 * @hide
 */
public class CsipSetCoordinatorScanner {
    private static final boolean DBG = true;
    private static final boolean VDBG = CsipSetCoordinatorService.VDBG;
    private static final String TAG = "CsipSetCoordinatorScanner";

    // messages for handling filtered PSRI Scan results
    private static final int MSG_HANDLE_LE_SCAN_RESULT = 0;

    // message for starting coordinated set discovery
    private static final int MSG_START_SET_DISCOVERY = 1;

    // message to stop coordinated set discovery
    private static final int MSG_STOP_SET_DISCOVERY = 2;

    // message when set member discovery timeout happens
    private static final int MSG_SET_MEMBER_DISC_TIMEOUT = 3;

    // message to handle PSRI from EIR packet
    private static final int MSG_HANDLE_EIR_RESPONSE = 4;

    // PSRI Service AD Type
    private final ParcelUuid PSRI_SERVICE_ADTYPE_UUID
            = BluetoothUuid.parseUuidFrom(new byte[]{0x2E, 0x00});

    private static final int PSRI_LEN = 6;
    private static final int PSRI_SPLIT_LEN = 3; // 24 bits
    private static final int AES_128_IO_LEN = 16;

    // Set Member Discovery timeout
    private static final int SET_MEMBER_DISCOVERY_TIMEOUT = 10000; // 10 sec

    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothDevice mCurrentDevice;
    private BluetoothLeScanner mScanner;
    private CsipSetCoordinatorService mCsipSetCoordinatorService;
    private volatile CsipHandler mHandler;
    private Handler mainHandler;
    private boolean mScanResolution;
    private int mDiscoveryStoppedReason;
    private CsipLeScanCallback mCsipScanCallback;

    // parameters for set discovery
    private int mSetId;
    private byte[] mSirk;
    private int mTransport;
    private int mSetSize;
    private int mTotalDiscovered;

    private int mScanType = 1;
    private Looper mLooper;

    // filter out duplicate scans
    ArrayList<BluetoothDevice> scannedDevices = new ArrayList<BluetoothDevice>();

    CsipSetCoordinatorScanner(CsipSetCoordinatorService service) {
        mCsipSetCoordinatorService = service;
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        // register receiver for Bluetooth State change
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        mCsipSetCoordinatorService.registerReceiver(mReceiver, filter);

        mainHandler = new Handler(mCsipSetCoordinatorService.getMainLooper());
        HandlerThread thread = new HandlerThread("CsipScanHandlerThread");
        thread.start();
        mLooper = thread.getLooper();
        mHandler = new CsipHandler(mLooper);
        mCsipScanCallback = new CsipLeScanCallback();
        ScanRecord.DATA_TYPE_GROUP_AD_TYPE = 0x2E;
        /* Testing: Property used for deciding scan and filter type. To be removed */
        mScanType = SystemProperties.getInt(
                     "persist.vendor.service.bt.csip.scantype", 1);
    }

    Looper getmLooper() {
        return mLooper;
    }

    // Handler for CSIP scan operations and set member resolution.
    private class CsipHandler extends Handler {
        CsipHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (VDBG) Log.v(TAG, "msg.what = " + msg.what);

            switch (msg.what) {
                case MSG_HANDLE_LE_SCAN_RESULT:
                    // start processing scan result
                    int callBackType = msg.arg1;
                    ScanResult result = (ScanResult) msg.obj;
                    mCurrentDevice = result.getDevice();

                    /* In case of DUMO device if advertisement is coming from other RPA */
                    if (mCurrentDevice.getBondState() == BluetoothDevice.BOND_BONDED) {
                        return;
                    }

                    // skip scanresult if already processed for this device
                    if (scannedDevices.contains(mCurrentDevice)) {
                        if (VDBG) {
                            Log.w(TAG, "duplicate scanned result or Device"
                                    + mCurrentDevice + " Group info already resolved. Ignore");
                         }
                        return;
                    }

                    scannedDevices.add(mCurrentDevice);
                    ScanRecord record =  result.getScanRecord();

                    // get required service data with PSRI AD Type
                    byte[] srvcData = null;
                    /* for debugging purpose */
                    if (mScanType == 2) {
                        srvcData = record.getServiceData(PSRI_SERVICE_ADTYPE_UUID);
                    } else {
                        srvcData = record.getGroupIdentifierData();
                    }
                    if (srvcData == null || srvcData.length != PSRI_LEN) {
                        Log.e(TAG, "Group info with incorrect length found "
                                + "in advertisement of " + mCurrentDevice);
                        return;
                    }

                    startPsriResolution(srvcData);
                    break;

                case MSG_HANDLE_EIR_RESPONSE:
                    EirData eirData = (EirData)msg.obj;
                    mCurrentDevice = eirData.curDevice;
                    byte[] eirGroupData = eirData.groupData;

                    // skip eir if already processed for this device
                    if (scannedDevices.contains(mCurrentDevice)) {
                        if (VDBG) {
                            Log.w(TAG, "duplicate eir or Device"
                                    + mCurrentDevice + " PSRI already resolved. Ignore");
                         }
                        return;
                    }

                    scannedDevices.add(mCurrentDevice);
                    if (eirGroupData == null || eirGroupData.length != PSRI_LEN) {
                        Log.e(TAG, "PSRI data with incorrect length found "
                                + "in EIR of " + mCurrentDevice);
                        return;
                    }
                    startPsriResolution(eirGroupData);
                    break;

                // High priority msg received in front of the message queue
                case MSG_SET_MEMBER_DISC_TIMEOUT:
                    mDiscoveryStoppedReason = BluetoothDeviceGroup.DISCOVERY_STOPPED_BY_TIMEOUT;
                case MSG_STOP_SET_DISCOVERY:
                    handleStopSetDiscovery();
                    break;

                // High priority msg received in front of the message queue
                case MSG_START_SET_DISCOVERY:
                    handleStartSetDiscovery();
                    break;

                default:
                    Log.e(TAG, "Unknown message : " + msg.what);
            }
        }
    }

    /* BroadcastReceiver for BT ON State intent for registering BLE Scanner */
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.e(TAG, "Received intent with null action");
                return;
            }

            switch (action) {
                case BluetoothAdapter.ACTION_STATE_CHANGED:
                    mScanner = mBluetoothAdapter.getBluetoothLeScanner();
                break;
            }

        }
    };

    /* Scan results callback */
    private class CsipLeScanCallback extends ScanCallback {
        @Override
        public void onScanResult(int callBackType, ScanResult result) {
            if (VDBG) Log.v(TAG, "onScanResult callBackType : " + callBackType);
            if (mHandler != null && mScanResolution) {
                mHandler.sendMessage(mHandler.obtainMessage(MSG_HANDLE_LE_SCAN_RESULT,
                        callBackType, 0, result));
             } else {
                if (VDBG) Log.e(TAG, "onScanResult mHandler is null" +
                                     " or Scan Resolution is stopped");
             }
        }

        public void onScanFailed(int errorCode) {
            mScanResolution = false;
            Log.e(TAG, "Scan failed. Error code: " + new Integer(errorCode).toString());
        }
    }

    /* EIR Data */
    private class EirData {
        private BluetoothDevice curDevice;
        private byte[] groupData;

        EirData(BluetoothDevice device, byte[] data) {
            curDevice = device;
            groupData = data;
        }
    }

    /* API that handles PSRI received from EIR */
    public void handleEIRGroupData(BluetoothDevice device, byte[] data) {
        if (VDBG) Log.v(TAG, "handleEirData: device: " + device);
        if (mHandler != null && mScanResolution) {
            EirData eirData = new EirData(device, data);
            mHandler.sendMessage(mHandler.obtainMessage(MSG_HANDLE_EIR_RESPONSE, eirData));
         } else {
            if (VDBG) Log.e(TAG, "handleEirData mHandler is null" +
                                 " or Inquiry Scan Resolution is stopped");
         }
    }

    /* API to start set discovery by starting either LE scan or BREDR Inquiry */
    void startSetDiscovery(int setId, byte[] sirk, int transport,
            int size, List<BluetoothDevice> setDevices) {
        Log.d(TAG, "startGroupDiscovery: groupId: " + setId + ", group size = "
                + size + ", Total discovered = " + setDevices.size()
                + " Transport = " + transport);

        // check if set discovery is already in progress
        if (mScanResolution) {
            Log.e(TAG, "Group discovery is already in progress for Group Id: " + mSetId
                    + ". Ignore this request");
            return;
        }

        // mark parameters of the set to be discovered
        mSetId = setId;
        mTransport = transport;
        mSetSize = size;
        mTotalDiscovered = setDevices.size();
        mSirk = Arrays.copyOf(sirk, AES_128_IO_LEN);
        reverseByteArray(mSirk);

        // clear scanned arrayList and add already found set members to it
        scannedDevices.clear();
        scannedDevices.addAll(setDevices);

        //post message in the front of message queue
        mHandler.sendMessageAtFrontOfQueue(
                mHandler.obtainMessage(MSG_START_SET_DISCOVERY));
    }

    /* API to start discovery with required settings and transport */
    void handleStartSetDiscovery() {
        Log.d(TAG, "handleStartGroupDiscovery");
        mScanResolution = true;

        if (mTransport == BluetoothDevice.DEVICE_TYPE_CLASSIC) {
            // start BREDR inquiry (unfiltered)
            mBluetoothAdapter.startDiscovery();
        } else {
            // Confiigure scan filter and start filtered scan for PSRI data
            ScanSettings.Builder settingBuilder = new ScanSettings.Builder();
            List<ScanFilter> filters = new ArrayList<ScanFilter>();
            byte[] psri = {};

            mScanType = SystemProperties.getInt(
                     "persist.vendor.service.bt.csip.scantype", 1);

            // for debugging purpose only
            if (mScanType == 2) {
                filters.add(new ScanFilter.Builder().setServiceData(
                        PSRI_SERVICE_ADTYPE_UUID, psri).build());
            } else if (mScanType == 1) {
                filters.add(new ScanFilter.Builder().setGroupBasedFiltering(true)
                                                    .build());
            }
            settingBuilder.setScanMode(ScanSettings.SCAN_MODE_BALANCED)
                          .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                          .setLegacy(false);

            // start BLE filtered Scan
            if (mScanType != 0) {
                Log.i(TAG, " filtered scan started");// debug
                mScanner.startScan(filters, settingBuilder.build(), mCsipScanCallback);
            } else {
                Log.i(TAG, " Unfiltered scan started");// debug
                mScanner.startScan(mCsipScanCallback);
            }
        }

        // Start Set Member discovery timeout of 10 sec
        mHandler.removeMessages(MSG_SET_MEMBER_DISC_TIMEOUT);
        mHandler.sendMessageDelayed(
                mHandler.obtainMessage(MSG_SET_MEMBER_DISC_TIMEOUT),
                SET_MEMBER_DISCOVERY_TIMEOUT);
    }

    /* to stop set discorvey procedure - stop LE scan or BREDR inquiry */
    void stopSetDiscovery(int setId, int reason) {
        Log.d(TAG, "stopGroupDiscovery");

        mDiscoveryStoppedReason = reason;

        //post message in the front of message queue
        mHandler.sendMessageAtFrontOfQueue(
                mHandler.obtainMessage(MSG_STOP_SET_DISCOVERY));
    }

    /* handles actions to be taken once set discovery is needed to be stopped*/
    void handleStopSetDiscovery() {
        Log.d(TAG, "handleStopGroupDiscovery");
        mScanResolution = false;

        if (mTransport == BluetoothDevice.DEVICE_TYPE_LE ||
                mTransport == BluetoothDevice.DEVICE_TYPE_DUAL) {
            mScanner.stopScan(mCsipScanCallback);
        } else {
            mBluetoothAdapter.cancelDiscovery();
        }

        // remove all the queued scan results and set member discovery timeout message
        mHandler.removeMessages(MSG_HANDLE_LE_SCAN_RESULT);
        mHandler.removeMessages(MSG_SET_MEMBER_DISC_TIMEOUT);

        // Give callback to service to route it to requesting application
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                mCsipSetCoordinatorService.onSetDiscoveryCompleted(
                        mSetId, mTotalDiscovered, mDiscoveryStoppedReason);
            }
        });
    }

    /* Starts resolution of PSRI data received in scan results */
    void startPsriResolution(byte[] psri) {
        Log.d(TAG, "startGroupResolution");

        if (VDBG) printByteArrayInHex(psri, "GroupInfo");
        // obtain remote hash and random number
        byte[] remoteHash = new byte[PSRI_SPLIT_LEN];
        byte[] randomNumber = new byte[PSRI_SPLIT_LEN];

        // Get remote hash from first 24 bits of PSRI
        System.arraycopy(psri, 0, remoteHash, 0, PSRI_SPLIT_LEN);
        // Get random number from last 24 bits of PSRI
        System.arraycopy(psri, PSRI_SPLIT_LEN, randomNumber, 0, PSRI_SPLIT_LEN);

        byte[] localHash = computeLocalHash(randomNumber);

        if (VDBG) {
            printByteArrayInHex(localHash, "localHash");
            printByteArrayInHex(remoteHash, "remoteHash");
        }

        if (localHash != null) {
            validateSetMember(localHash, remoteHash);
        }
    }

    /* computes local hash from received random number and SIRK */
    byte[] computeLocalHash(byte[] randomNumber) {
        byte[] localHash = new byte[AES_128_IO_LEN];
        byte[] randomNumber128 = new byte[AES_128_IO_LEN];
        System.arraycopy(randomNumber, 0, randomNumber128, 0, PSRI_SPLIT_LEN);

        reverseByteArray(randomNumber128);

        if (VDBG) {
            // for debugging
            printByteArrayInHex(mSirk, "reversed GroupIRK");
            printByteArrayInHex(randomNumber128, "reverse randomNumber");
        }

        try {
            SecretKeySpec skeySpec = new SecretKeySpec(mSirk, "AES");
            Cipher cipher = Cipher.getInstance("AES/ECB/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, skeySpec);
            localHash = cipher.doFinal(randomNumber128);
            reverseByteArray(localHash);
            if (VDBG) printByteArrayInHex(localHash, "after AES 128 encryption");
            return Arrays.copyOfRange(localHash, 0, PSRI_SPLIT_LEN);
        } catch (Exception e) {
            Log.e(TAG, "Exception while generating local hash: " + e);
        }
        return null;
    }

    /* to validate that if remote belongs to a given coordinated set*/
    void validateSetMember(byte[] localHash, byte[] remoteHash) {
        if (!Arrays.equals(localHash, remoteHash)) {
            return;
        }
        Log.d(TAG, "New Group device discovered: " + mCurrentDevice);
        mTotalDiscovered++;
        onSetMemberFound(mSetId, mCurrentDevice);
        //check if all set members have been discovered
        if (mSetSize > 0 && mTotalDiscovered >= mSetSize) {
            // to immediately ignore processing scan results after completion
            mScanResolution = false;
            mDiscoveryStoppedReason = BluetoothDeviceGroup.DISCOVERY_COMPLETED;
            mHandler.sendMessageAtFrontOfQueue(
                mHandler.obtainMessage(MSG_STOP_SET_DISCOVERY));
        } else {
            // restart set member discovery timeout
            mHandler.removeMessages(MSG_SET_MEMBER_DISC_TIMEOUT);
            mHandler.sendMessageDelayed(
                    mHandler.obtainMessage(MSG_SET_MEMBER_DISC_TIMEOUT),
                    SET_MEMBER_DISCOVERY_TIMEOUT);
        }

    }

    /* cleanup tasks on BT OFF*/
    void cleanup() {
        mCsipSetCoordinatorService.unregisterReceiver(mReceiver);
    }

    // returns reversed byte array
    void reverseByteArray(byte[] byte_arr) {
        int size = byte_arr.length;
        for (int i = 0; i < size/2; i++) {
            byte b = byte_arr[i];
            byte_arr[i] = byte_arr[size - 1 - i];
            byte_arr[size - 1 - i] = b;
        }
    }

    public static byte[] hexStringToByteArray(String str) {
        byte[] b = new byte[str.length() / 2];
        for (int i = 0; i < b.length; i++) {
          int index = i * 2;
          int val = Integer.parseInt(str.substring(index, index + 2), 16);
          b[i] = (byte) val;
        }
        return b;
      }

    // print byte array in hexadecimal format
    void printByteArrayInHex(byte[] data, String name) {
        final StringBuilder hex = new StringBuilder();
        for(byte b : data) {
            hex.append(String.format("%02x", b));
        }
        Log.i(TAG, name + ": " + hex);
    }

    /* Starts resolution of PSRI data received in opt scan results */
    boolean startPsriResolution(byte[] psri, byte []sirk, BluetoothDevice device, int setid) {
        if (VDBG) Log.v(TAG, "startPsriResolution from rsi");
        byte [] sirkTemp = Arrays.copyOf(sirk, AES_128_IO_LEN);
        reverseByteArray(sirkTemp);
        if (VDBG) printByteArrayInHex(psri, "GroupInfo");
        byte[] remoteHash = new byte[PSRI_SPLIT_LEN];
        byte[] randomNumber = new byte[PSRI_SPLIT_LEN];
        System.arraycopy(psri, 0, remoteHash, 0, PSRI_SPLIT_LEN);
        System.arraycopy(psri, PSRI_SPLIT_LEN, randomNumber, 0, PSRI_SPLIT_LEN);
        byte[] localHash = computeLocalHash(randomNumber, sirkTemp);
        if (VDBG) {
            printByteArrayInHex(localHash, "localHash");
            printByteArrayInHex(remoteHash, "remoteHash");
        }
        if (localHash != null) {
            return validateSetMember(localHash, remoteHash, device, setid);
        }
        return false;
    }

    byte[] computeLocalHash(byte[] randomNumber, byte [] sirk) {
        if (VDBG) Log.v(TAG, "computeLocalHash from rsi");
        byte[] localHash = new byte[AES_128_IO_LEN];
        byte[] randomNumber128 = new byte[AES_128_IO_LEN];
        System.arraycopy(randomNumber, 0, randomNumber128, 0, PSRI_SPLIT_LEN);
        reverseByteArray(randomNumber128);
        if (VDBG) {
            printByteArrayInHex(sirk, "reversed GroupIRK");
            printByteArrayInHex(randomNumber128, "reverse randomNumber");
        }
        try {
            SecretKeySpec skeySpec = new SecretKeySpec(sirk, "AES");
            Cipher cipher = Cipher.getInstance("AES/ECB/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, skeySpec);
            localHash = cipher.doFinal(randomNumber128);
            reverseByteArray(localHash);
            if (VDBG) printByteArrayInHex(localHash, "after AES 128 encryption");
            return Arrays.copyOfRange(localHash, 0, PSRI_SPLIT_LEN);
        } catch (Exception e) {
            Log.e(TAG, "Exception while generating local hash: " + e);
        }
        return null;
    }

    /* Validate that if remote belongs to a given coordinated set*/
    boolean validateSetMember(byte[] localHash, byte[] remoteHash, BluetoothDevice device,
            int setid) {
        if (!Arrays.equals(localHash, remoteHash)) {
            return false;
        }
        if (DBG) Log.d(TAG, "Set member discovered from rsi " + device);
        onSetMemberFound(setid, device);
        return true;
    }

    // give set member found callback on main thread
    private void onSetMemberFound(final int setid, BluetoothDevice device) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                mCsipSetCoordinatorService.onSetMemberFound(setid, device);
            }
        });
    }
}

