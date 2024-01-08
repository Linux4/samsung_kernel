/*
 * Copyright (C) 2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 * * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2012-2014 The Android Open Source Project
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

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package com.android.bluetooth.btservice;

import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.Manifest.permission.BLUETOOTH_SCAN;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAssignedNumbers;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothConnectionCallback;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.MacAddress;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.util.Log;

import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.CsipWrapper;
import com.android.bluetooth.R;
import com.android.bluetooth.ReflectionUtils;
import com.android.bluetooth.Utils;
import com.android.bluetooth.hfp.HeadsetHalConstants;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.ArrayUtils;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Queue;
import java.util.Set;
import java.util.function.Predicate;

final class RemoteDevices {
    private static final boolean DBG = true;
    private static final String TAG = "BluetoothRemoteDevices";

    // Maximum number of device properties to remember
    private static final int MAX_DEVICE_QUEUE_SIZE = 200;

    private static BluetoothAdapter sAdapter;
    private static AdapterService sAdapterService;
    private static boolean isServiceInit = false;
    private static ArrayList<BluetoothDevice> sSdpTracker;
    private final Object mObject = new Object();

    private static final int UUID_INTENT_DELAY = 6000;
    private static final int MESSAGE_UUID_INTENT = 1;

    private final HashMap<String, DeviceProperties> mDevices;
    private Queue<String> mDeviceQueue;
    private CsipWrapper mCsipWrapper;
    private final Handler mHandler;
    private class RemoteDevicesHandler extends Handler {

        /**
         * Handler must be created from an explicit looper to avoid threading ambiguity
         * @param looper The looper that this handler should be executed on
         */
        RemoteDevicesHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MESSAGE_UUID_INTENT:
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (device != null) {
                        DeviceProperties prop = getDeviceProperties(device);
                        sendUuidIntent(device, prop);
                    }
                    break;
            }
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            switch (action) {
                case BluetoothHeadset.ACTION_HF_INDICATORS_VALUE_CHANGED:
                    onHfIndicatorValueChanged(intent);
                    break;
                case BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT:
                    onVendorSpecificHeadsetEvent(intent);
                    break;
                case BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED:
                    onHeadsetConnectionStateChanged(intent);
                    break;
                default:
                    Log.w(TAG, "Unhandled intent: " + intent);
                    break;
            }
        }
    };

    public static final String ACTION_TWS_PLUS_DEVICE_PAIR =
        "android.bluetooth.device.action.TWS_PLUS_DEVICE_PAIR";
    public static final String EXTRA_TWS_PLUS_DEVICE1 =
        "android.bluetooth.device.extra.EXTRA_TWS_PLUS_DEVICE1";
    public static final String EXTRA_TWS_PLUS_DEVICE2 =
        "android.bluetooth.device.extra.EXTRA_TWS_PLUS_DEVICE2";
    /**
     * Predicate that tests if the given {@link BluetoothDevice} is well-known
     * to be used for physical location.
     */
    private final Predicate<BluetoothDevice> mLocationDenylistPredicate = (device) -> {
        final MacAddress parsedAddress = MacAddress.fromString(device.getAddress());
        if (sAdapterService.getLocationDenylistMac().test(parsedAddress.toByteArray())) {
            Log.v(TAG, "Skipping device matching denylist: " + parsedAddress);
            return true;
        }
        final String name = Utils.getName(device);
        if (sAdapterService.getLocationDenylistName().test(name)) {
            Log.v(TAG, "Skipping name matching denylist: " + name);
            return true;
        }
        return false;
    };
    RemoteDevices(AdapterService service, Looper looper) {
        sAdapter = BluetoothAdapter.getDefaultAdapter();
        sAdapterService = service;
        sSdpTracker = new ArrayList<BluetoothDevice>();
        mDevices = new HashMap<String, DeviceProperties>();
        mDeviceQueue = new LinkedList<String>();
        mHandler = new RemoteDevicesHandler(looper);
        mCsipWrapper = CsipWrapper.getInstance();
    }

    /**
     * Init should be called before using this RemoteDevices object
     */
    void init() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothHeadset.ACTION_HF_INDICATORS_VALUE_CHANGED);
        filter.addAction(BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT);
        filter.addCategory(BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_COMPANY_ID_CATEGORY + "."
                + BluetoothAssignedNumbers.PLANTRONICS);
        filter.addCategory(BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_COMPANY_ID_CATEGORY + "."
                + BluetoothAssignedNumbers.APPLE);
        filter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        sAdapterService.registerReceiver(mReceiver, filter);
    }

    /**
     * Clean up should be called when this object is no longer needed, must be called after init()
     */
    void cleanup() {
        // Unregister receiver first, mAdapterService is never null
        sAdapterService.unregisterReceiver(mReceiver);
        reset();
    }

    /**
     * Reset should be called when the state of this object needs to be cleared
     * RemoteDevices is still usable after reset
     */
    void reset() {
        if (sSdpTracker != null) {
            sSdpTracker.clear();
        }

        if (mDevices != null) {
            mDevices.clear();
        }

        if (mDeviceQueue != null) {
            mDeviceQueue.clear();
        }
    }

    @Override
    public Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    DeviceProperties getDeviceProperties(BluetoothDevice device) {
        synchronized (mDevices) {
            return mDevices.get(device.getAddress());
        }
    }

    BluetoothDevice getDevice(byte[] address) {
        DeviceProperties prop;
        synchronized (mDevices) {
            prop = mDevices.get(Utils.getAddressStringFromByte(address));
        }
        if (prop != null) {
            return prop.getDevice();
        }
        return null;
    }

    @VisibleForTesting
    DeviceProperties addDeviceProperties(byte[] address) {
        synchronized (mDevices) {
            DeviceProperties prop = new DeviceProperties();
            prop.mDevice = sAdapter.getRemoteDevice(Utils.getAddressStringFromByte(address));
            prop.mAddress = address;
            String key = Utils.getAddressStringFromByte(address);
            DeviceProperties pv = mDevices.put(key, prop);

            if (pv == null) {
                mDeviceQueue.offer(key);
                if (mDeviceQueue.size() > MAX_DEVICE_QUEUE_SIZE) {
                    String deleteKey = mDeviceQueue.poll();
                    for (BluetoothDevice device : sAdapterService.getBondedDevices()) {
                        if (device.getAddress().equals(deleteKey)) {
                            return prop;
                        }
                    }

                    BluetoothDevice newdevice = getDevice(Utils.addressToBytes(deleteKey));
                    DeviceProperties deviceProperties = getDeviceProperties(newdevice);
                    if (deviceProperties != null && deviceProperties.isBonding()) {
                        debugLog("Bonding device " + deleteKey + " Don't remove from property map");
                        return prop;
                    }
                    debugLog("Removing device " + deleteKey + " from property map");
                    mDevices.remove(deleteKey);
                }
            }
            return prop;
        }
    }

    class DeviceProperties {
        private String mName;
        private byte[] mAddress;
        private int mBluetoothClass = BluetoothClass.Device.Major.UNCATEGORIZED;
        private short mRssi;
        private String mAlias;
        private BluetoothDevice mDevice;
        private boolean mIsBondingInitiatedLocally;
        private int mBatteryLevel = BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        private short mTwsPlusDevType;
        private byte[] peerEbAddress;
        private boolean autoConnect;
        private boolean mSdpProgress;
        public final ParcelUuid BR_TRANSPORT_UUID =
            ParcelUuid.fromString("87564312-0000-1000-8000-00805F9B34FB");
        public final ParcelUuid LE_TRANSPORT_UUID =
            ParcelUuid.fromString("87564313-0000-1000-8000-00805F9B34FB");
        @VisibleForTesting int mBondState;
        @VisibleForTesting int mDeviceType;
        @VisibleForTesting ParcelUuid[] mUuids;
        @VisibleForTesting int mUuidTransport;
        @VisibleForTesting int mBdAddrValid;
        @VisibleForTesting ArrayList<ParcelUuid> mAdvAudioUuids;
        @VisibleForTesting boolean mAdvAudioUpdateProp;
        @VisibleForTesting byte[] mMapBdAddress;
        @VisibleForTesting HashMap<ParcelUuid, Integer> uuidsByTransport;
        private boolean mIsCoordinatedSetMember;

        DeviceProperties() {
            mBondState = BluetoothDevice.BOND_NONE;
            mTwsPlusDevType = AbstractionLayer.TWS_PLUS_DEV_TYPE_NONE;
            autoConnect = true;
            mSdpProgress = true;
            peerEbAddress = null;
            mBdAddrValid = 1;
            mAdvAudioUuids = new ArrayList<ParcelUuid>();
            mAdvAudioUpdateProp = true;
            uuidsByTransport = new HashMap<ParcelUuid, Integer>();
            mUuids = null;
        }

        /**
         * @return the mName
         */
        String getName() {
            synchronized (mObject) {
                return mName;
            }
        }

        /**
         * @return the mClass
         */
        int getBluetoothClass() {
            synchronized (mObject) {
                return mBluetoothClass;
            }
        }

        /**
         * @return the mUuids
         */
        ParcelUuid[] getUuids() {
            synchronized (mObject) {
                return mUuids;
            }
        }

        /**
         * @return the mAddress
         */
        byte[] getAddress() {
            synchronized (mObject) {
                return mAddress;
            }
        }

        /**
         * @return the mDevice
         */
        BluetoothDevice getDevice() {
            synchronized (mObject) {
                return mDevice;
            }
        }

        /**
         * @return mRssi
         */
        short getRssi() {
            synchronized (mObject) {
                return mRssi;
            }
        }

        /**
         * @return mDeviceType
         */
        int getDeviceType() {
            synchronized (mObject) {
                return mDeviceType;
            }
        }

        /**
         * @return the mAlias
         */
        String getAlias() {
            synchronized (mObject) {
                return mAlias;
            }
        }

        /**
         * @param mAlias the mAlias to set
         */
        void setAlias(BluetoothDevice device, String mAlias) {
            synchronized (mObject) {
                this.mAlias = mAlias;
                if (mAlias == null)
                    return;
                sAdapterService.setDevicePropertyNative(mAddress,
                        AbstractionLayer.BT_PROPERTY_REMOTE_FRIENDLY_NAME, mAlias.getBytes());
                Intent intent = new Intent(BluetoothDevice.ACTION_ALIAS_CHANGED);
                intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                intent.putExtra(BluetoothDevice.EXTRA_NAME, mAlias);
                sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                        Utils.getTempAllowlistBroadcastOptions());                
            }
        }

        /**
         * @return mTwsPlusDevType
         */
        int getTwsPlusDevType() {
            synchronized (mObject) {
                return mTwsPlusDevType;
            }
        }

        /**
         * @return peerEbAddress
         */
        byte[] getTwsPlusPeerAddress() {
            synchronized (mObject) {
                return peerEbAddress;
            }
        }

        /**
         * @param mTwsPlusDevType the mTwsPlusDevType to set
         */
        void setTwsPlusDevType(short twsPlusDevType) {
            synchronized (mObject) {
                this.mTwsPlusDevType = twsPlusDevType;
                if(twsPlusDevType == AbstractionLayer.TWS_PLUS_DEV_TYPE_NONE) {
                   this.peerEbAddress = null;
                }
            }
        }

        /**
         * @param peerEbAddress the peerEbAddress to set
         */
        void setTwsPlusPeerEbAddress(BluetoothDevice device, byte[] peerEbAddress) {
            synchronized (mObject) {
                Intent intent;

                /* in case of null null bd address reset the address */
                if (peerEbAddress != null &&
                    Utils.getAddressStringFromByte(peerEbAddress).equals("00:00:00:00:00:00")) {
                    this.peerEbAddress = null;
                    errorLog(" resetting the peerEbAddress to null");
                } else {
                    this.peerEbAddress = peerEbAddress;
                    if(device != null && peerEbAddress != null) {
                        errorLog(" Peer EB Address is:" +
                                Utils.getAddressStringFromByte(peerEbAddress));
                        intent = new Intent(ACTION_TWS_PLUS_DEVICE_PAIR);
                        intent.putExtra(EXTRA_TWS_PLUS_DEVICE1, mDevice);
                        intent.putExtra(EXTRA_TWS_PLUS_DEVICE2, device);
                        sAdapterService.sendBroadcast(intent,
                                BLUETOOTH_CONNECT);
                    }
                }
            }
        }

        /**
         * @param peerEbAddress the peerEbAddress to set
         */
        void setTwsPlusAutoConnect(BluetoothDevice device, boolean autoConnect) {
            synchronized (mObject) {
                this.autoConnect = autoConnect;
                debugLog("sendUuidIntent as Auto connect  " + autoConnect );
            }
        }
        /*
         * @param mBondState the mBondState to set
         */
        void setBondState(int mBondState) {
            synchronized (mObject) {
                this.mBondState = mBondState;
                if (mBondState == BluetoothDevice.BOND_NONE) {
                    /* Clearing the Uuids local copy when the device is unpaired. If not cleared,
                    cachedBluetoothDevice issued a connect using the local cached copy of uuids,
                    without waiting for the ACTION_UUID intent.
                    This was resulting in multiple calls to connect().*/
                    mUuids = null;
                }
            }
        }

        /**
         * @return the mBondState
         */
        int getBondState() {
            synchronized (mObject) {
                return mBondState;
            }
        }

        boolean isBonding() {
            return getBondState() == BluetoothDevice.BOND_BONDING;
        }

        boolean isBondingOrBonded() {
            return isBonding() || getBondState() == BluetoothDevice.BOND_BONDED;
        }

        /**
         * @param isBondingInitiatedLocally wether bonding is initiated locally
         */
        void setBondingInitiatedLocally(boolean isBondingInitiatedLocally) {
            synchronized (mObject) {
                this.mIsBondingInitiatedLocally = isBondingInitiatedLocally;
            }
        }

        /**
         * @return the isBondingInitiatedLocally
         */
        boolean isBondingInitiatedLocally() {
            synchronized (mObject) {
                return mIsBondingInitiatedLocally;
            }
        }

        int getBatteryLevel() {
            synchronized (mObject) {
                return mBatteryLevel;
            }
        }

        int getValidBDAddr() {
            synchronized (mObject) {
                return mBdAddrValid;
            }
        }

        byte[] getMappingAddr() {
            synchronized (mObject) {
                debugLog(" getMappingAddr " + mMapBdAddress);
                return mMapBdAddress;
            }
        }

        int getUuidTransport (ParcelUuid uuid) {
            synchronized (mObject) {
                debugLog(" getUuidTransport ");
                return (uuidsByTransport.get(uuid).intValue());
            }
        }

        /**
         * @param batteryLevel the mBatteryLevel to set
         */
        void setBatteryLevel(int batteryLevel) {
            synchronized (mObject) {
                this.mBatteryLevel = batteryLevel;
            }
        }

        /**
         * @return the mIsCoordinatedSetMember
        */
        boolean isCoordinatedSetMember() {
            synchronized (mObject) {
                return mIsCoordinatedSetMember;
            }
        }

        void setDefaultBDAddrValidType() {
            synchronized (mObject) {
              this.mBdAddrValid = 1;
            }
        }

        void setBluetoothClass(int mBluetoothClass) {
            synchronized (mObject) {
                this.mBluetoothClass = mBluetoothClass;
            }
        }

        void setSdpProgress(boolean sdpProgress) {
            synchronized (mObject) {
              if (mSdpProgress != sdpProgress) {
                  mSdpProgress = sdpProgress;
              }
            }
        }

        boolean isSdpCompleted() {
            synchronized (mObject) {
                return mSdpProgress;
            }
        }
    }

    private void sendUuidIntent(BluetoothDevice device, DeviceProperties prop) {
        if (sAdapterService.isIgnoreDevice(device)) {
            debugLog("Ignoring action_UUID " +device.getAddress());
            return;
        }
        Intent intent = new Intent(BluetoothDevice.ACTION_UUID);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDevice.EXTRA_UUID, prop == null ? null : prop.mUuids);
        sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());

        //Remove the outstanding UUID request
        if (sSdpTracker.contains(device)) {
            sSdpTracker.remove(device);
        }
    }

    /**
     * When bonding is initiated to remote device that we have never seen, i.e Out Of Band pairing,
     * we must add device first before setting it's properties. This is a helper method for doing
     * that.
     */
    void setBondingInitiatedLocally(byte[] address) {
        DeviceProperties properties;

        BluetoothDevice device = getDevice(address);
        if (device == null) {
            properties = addDeviceProperties(address);
        } else {
            properties = getDeviceProperties(device);
        }

        properties.setBondingInitiatedLocally(true);
    }

    /**
     * Update battery level in device properties
     * @param device The remote device to be updated
     * @param batteryLevel Battery level Indicator between 0-100,
     *                    {@link BluetoothDevice#BATTERY_LEVEL_UNKNOWN} is error
     */
    @VisibleForTesting
    void updateBatteryLevel(BluetoothDevice device, int batteryLevel) {
        if (device == null || batteryLevel < 0 || batteryLevel > 100) {
            warnLog("Invalid parameters device=" + String.valueOf(device == null)
                    + ", batteryLevel=" + String.valueOf(batteryLevel));
            return;
        }
        DeviceProperties deviceProperties = getDeviceProperties(device);
        if (deviceProperties == null) {
            deviceProperties = addDeviceProperties(Utils.getByteAddress(device));
        }
        synchronized (mObject) {
            int currentBatteryLevel = deviceProperties.getBatteryLevel();
            if (batteryLevel == currentBatteryLevel) {
                debugLog("Same battery level for device " + device + " received " + String.valueOf(
                        batteryLevel) + "%");
                return;
            }
            deviceProperties.setBatteryLevel(batteryLevel);
        }
        sendBatteryLevelChangedBroadcast(device, batteryLevel);
        Log.d(TAG, "Updated device " + device + " battery level to " + batteryLevel + "%");
    }

    /**
     * Reset battery level property to {@link BluetoothDevice#BATTERY_LEVEL_UNKNOWN} for a device
     * @param device device whose battery level property needs to be reset
     */
    @VisibleForTesting
    void resetBatteryLevel(BluetoothDevice device) {
        if (device == null) {
            warnLog("Device is null");
            return;
        }
        DeviceProperties deviceProperties = getDeviceProperties(device);
        if (deviceProperties == null) {
            return;
        }
        synchronized (mObject) {
            if (deviceProperties.getBatteryLevel() == BluetoothDevice.BATTERY_LEVEL_UNKNOWN) {
                debugLog("Battery level was never set or is already reset, device=" + device);
                return;
            }
            deviceProperties.setBatteryLevel(BluetoothDevice.BATTERY_LEVEL_UNKNOWN);
        }
        sendBatteryLevelChangedBroadcast(device, BluetoothDevice.BATTERY_LEVEL_UNKNOWN);
        Log.d(TAG, "Reset battery level, device=" + device);
    }

    private void sendBatteryLevelChangedBroadcast(BluetoothDevice device, int batteryLevel) {
        Intent intent = new Intent(BluetoothDevice.ACTION_BATTERY_LEVEL_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDevice.EXTRA_BATTERY_LEVEL, batteryLevel);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    private static boolean areUuidsEqual(ParcelUuid[] uuids1, ParcelUuid[] uuids2) {
        final int length1 = uuids1 == null ? 0 : uuids1.length;
        final int length2 = uuids2 == null ? 0 : uuids2.length;
        if (length1 != length2) {
            return false;
        }
        Set<ParcelUuid> set = new HashSet<>();
        for (int i = 0; i < length1; ++i) {
            set.add(uuids1[i]);
        }
        for (int i = 0; i < length2; ++i) {
            set.remove(uuids2[i]);
        }
        return set.isEmpty();
    }

    void devicePropertyChangedCallback(byte[] address, int[] types, byte[][] values) {
        Intent intent;
        byte[] val;
        int type;
        BluetoothDevice bdDevice = getDevice(address);
        DeviceProperties device;
        if (bdDevice == null) {
            debugLog("Added new device property");
            device = addDeviceProperties(address);
            bdDevice = getDevice(address);
        } else {
            device = getDeviceProperties(bdDevice);
        }

        if (device == null) {
            errorLog("device null ");
            return;
        }

        if (types.length <= 0) {
            errorLog("No properties to update");
            return;
        }
        for (int j = 0; j < types.length; j++) {
            type = types[j];
            val = values[j];
            if (val.length > 0) {
                synchronized (mObject) {
                    debugLog("Property type: " + type);
                    switch (type) {
                        case AbstractionLayer.BT_PROPERTY_BDNAME:
                            final String newName = new String(val);
                            if (newName.equals(device.mName)) {
                                debugLog("Skip name update for " + bdDevice);
                                break;
                            }
                            if(sAdapterService.isIgnoreDevice(bdDevice)) {
                                debugLog("Skip name update for Duplicate Addr" + bdDevice);
                                break;
                            }
                            device.mName = newName;
                            intent = new Intent(BluetoothDevice.ACTION_NAME_CHANGED);
                            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, bdDevice);
                            intent.putExtra(BluetoothDevice.EXTRA_NAME, device.mName);
                            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
                            intent.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
                            sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                                    Utils.getTempAllowlistBroadcastOptions());
                            debugLog("Remote Device name is: " + device.mName);
                            break;
                        case AbstractionLayer.BT_PROPERTY_REMOTE_FRIENDLY_NAME:
                            device.mAlias = new String(val);
                            debugLog("Remote device alias is: " + device.mAlias);
                            break;
                        case AbstractionLayer.BT_PROPERTY_BDADDR:
                            device.mAddress = val;
                            debugLog("Remote Address is:" + Utils.getAddressStringFromByte(val));
                            break;
                        case AbstractionLayer.BT_PROPERTY_CLASS_OF_DEVICE:
                            final int newClass = Utils.byteArrayToInt(val);
                            if (newClass == device.mBluetoothClass) {
                              debugLog("Skip class update for " + bdDevice);
                              break;
                            }
                            int tmpBluetoothClass = device.getBluetoothClass();
                            debugLog("BT_PROPERTY_CLASS_OF_DEVICE:"
                                + tmpBluetoothClass + " " + bdDevice);
                            device.mBluetoothClass = newClass;
                            if (tmpBluetoothClass
                                        != BluetoothClass.Device.Major.UNCATEGORIZED
                                        && mCsipWrapper.isCsipEnabled() ) {
                                if ((tmpBluetoothClass & BluetoothClass.Service.LE_AUDIO)
                                        == BluetoothClass.Service.LE_AUDIO) {
                                    debugLog("Updated Adv class is:"
                                        + device.mBluetoothClass + " "+ device);
                                    device.mBluetoothClass |= BluetoothClass.Service.LE_AUDIO;
                                }
                            }
                            intent = new Intent(BluetoothDevice.ACTION_CLASS_CHANGED);
                            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, bdDevice);
                            intent.putExtra(BluetoothDevice.EXTRA_CLASS,
                                new BluetoothClass(device.mBluetoothClass));
                            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
                            sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                                    Utils.getTempAllowlistBroadcastOptions());
                            debugLog("Remote class is:" + device.mBluetoothClass);
                            break;
                        case AbstractionLayer.BT_PROPERTY_UUIDS:
                            if (device.mAdvAudioUpdateProp) {
                                int numUuids = val.length / AbstractionLayer.BT_UUID_SIZE;
                                final ParcelUuid[] newUuids = Utils.byteArrayToUuid(val);
                                if (areUuidsEqual(newUuids, device.mUuids)) {
                                    debugLog( "Skip uuids update for " + bdDevice.getAddress());
                                    break;
                                }
                                debugLog(" BT_PROPERTY_UUIDS " + bdDevice.getAddress());
                                device.mUuids = newUuids;
                                if ((sAdapterService.getState() == BluetoothAdapter.STATE_ON) &&
                                                                device.autoConnect) {
                                    debugLog("sendUuidIntent as Auto connect is set ");
                                    sAdapterService.deviceUuidUpdated(bdDevice);
                                    sendUuidIntent(bdDevice, device);
                                }
                            } else {
                                debugLog(" ADV_AUDIO DEVICE Skip BT_PROPERTY_UUIDS "
                                    + bdDevice.getAddress());
                            }
                            break;
                        case AbstractionLayer.BT_PROPERTY_ADV_AUDIO_UUIDS:
                            device.mAdvAudioUpdateProp = false;
                            int leNumUuids = val.length / AbstractionLayer.BT_UUID_SIZE;
                            final ParcelUuid[] leNewUuids = Utils.byteArrayToUuid(val);
                            if (areUuidsEqual(leNewUuids, device.mUuids)) {
                                debugLog( "Skip uuids update for " + bdDevice.getAddress());
                                break;
                            }
                            for (int inx = 0; inx < leNewUuids.length; inx++) {
                                if (!(device.mAdvAudioUuids.contains(leNewUuids[inx])))
                                    device.mAdvAudioUuids.add(leNewUuids[inx]);
                            }
                            debugLog( "ADV AUDIO UUIDS Update "
                                + bdDevice.getAddress() + "Num UUIDs " + device.mAdvAudioUuids.size());
                            break;
                        case AbstractionLayer.BT_PROPERTY_ADV_AUDIO_ACTION_UUID:
                            ParcelUuid[] tmpUuidArr =
                                device.mAdvAudioUuids.toArray(new ParcelUuid[device.mAdvAudioUuids.size()]);
                            int existUuidLen = 0;
                            if (device.mUuids != null) {
                                existUuidLen = device.mUuids.length;
                            }
                            debugLog( "--- Existing UuidLen  " + existUuidLen);
                            if (existUuidLen != 0) {
                                if (areUuidsEqual(tmpUuidArr, device.mUuids)) {
                                    debugLog( "--- Skip uuids update for " + bdDevice.getAddress());
                                } else {
                                    debugLog( "--- Updating Existing Uuids  ");
                                    ParcelUuid[] compltUuid = new ParcelUuid[tmpUuidArr.length + existUuidLen];
                                    System.arraycopy(device.mUuids, 0, compltUuid, 0, existUuidLen);
                                    System.arraycopy(tmpUuidArr, 0, compltUuid, existUuidLen, tmpUuidArr.length);
                                    device.mUuids = compltUuid;
                                }
                            }  else {
                                device.mUuids = tmpUuidArr;
                            }
                            debugLog("BT_PROPERTY_ADV_AUDIO_ACTION_UUID SiZE "
                                + tmpUuidArr.length +
                                " Device uuid size " + device.mUuids.length);
                            device.mAdvAudioUuids.clear();
                            if (!device.isBondingInitiatedLocally()) {
                              device.setBondingInitiatedLocally(true);
                            }
                            if ((sAdapterService.getState() == BluetoothAdapter.STATE_ON) &&
                                                            device.autoConnect ) {
                                //TODO remove log
                                debugLog("sendUuidIntent as Auto connect is set for Adv AUDIO");
                                sAdapterService.deviceUuidUpdated(bdDevice);
                                if (!sAdapterService.isIgnoreDevice(bdDevice)) {
                                  sendUuidIntent(bdDevice, device);
                                } else {
                                  debugLog("Ignoring action_UUID " +bdDevice.getAddress());
                                }
                            }
                            device.mAdvAudioUpdateProp = true;
                            break;
                        case AbstractionLayer.BT_PROPERTY_TYPE_OF_DEVICE:
                            debugLog("BT_PROPERTY_TYPE_OF_DEVICE " + bdDevice.getAddress());
                            // The device type from hal layer, defined in bluetooth.h,
                            // matches the type defined in BluetoothDevice.java
                            device.mDeviceType = Utils.byteArrayToInt(val);
                            break;
                        case AbstractionLayer.BT_PROPERTY_ADV_AUDIO_UUID_TRANSPORT:
                            debugLog("BT_PROPERTY_ADV_AUDIO_UUID_TRANSPORT "
                                + bdDevice.getAddress());
                            device.mUuidTransport = Utils.byteArrayToInt(val);
                            break;
                        case AbstractionLayer.BT_PROPERTY_ADV_AUDIO_VALID_ADDR_TYPE:
                            device.mBdAddrValid = Utils.byteArrayToInt(val);
                            debugLog("BT_PROPERTY_ADV_AUDIO_VALID_ADDR_TYPE "
                                + bdDevice.getAddress() + " addrValid "
                                + device.mBdAddrValid);
                            break;
                        case AbstractionLayer.BT_PROPERTY_REM_DEV_IDENT_BD_ADDR:
                            device.mMapBdAddress = val;
                            debugLog("BT_PROPERTY_REM_DEV_IDENT_BD_ADDR Remote Address MAP is:"
                                + Utils.getAddressStringFromByte(val));
                            break;

                        case AbstractionLayer.BT_PROPERTY_REMOTE_RSSI:
                            // RSSI from hal is in one byte
                            device.mRssi = val[0];
                            break;
                        case AbstractionLayer.BT_PROPERTY_REMOTE_DEVICE_GROUP:
                            mCsipWrapper.loadDeviceGroupFromBondedDevice(bdDevice, new String(val));
                            break;
                        case AbstractionLayer.BT_PROPERTY_GROUP_EIR_DATA:
                            mCsipWrapper.handleEIRGroupData(bdDevice, new String(val));
                            break;
                        case AbstractionLayer.BT_PROPERTY_ADV_AUDIO_UUID_BY_TRANSPORT:
                        {
                            int numTransUuids = val.length / AbstractionLayer.BT_UUID_SIZE;
                            final ParcelUuid[] transUuids = Utils.byteArrayToUuid(val);
                            int bredrTransIndex = ArrayUtils.indexOf(transUuids,
                                                    device.BR_TRANSPORT_UUID);
                            int leTransIndex = ArrayUtils.indexOf(transUuids,
                                                    device.LE_TRANSPORT_UUID);
                            debugLog("BT_PROPERTY_ADV_AUDIO_UUID_BY_TRANSPORT Num UUIDS :"
                                + numTransUuids + " bredrTransIndex "+ bredrTransIndex +
                                "leTransIndex " + leTransIndex);
                            int i, leInx = 0;
                            if (bredrTransIndex != -1) {
                                if (leTransIndex == -1)
                                    leInx = numTransUuids;
                                else
                                    leInx = leTransIndex;
                                //TODO Move this common functionality to separate API
                                for (i = 1; i < leInx; i++) {
                                    if (device.uuidsByTransport.containsKey(transUuids[i])) {
                                        int getTransport =
                                                device.uuidsByTransport.get(transUuids[i]).intValue();
                                        getTransport = (getTransport |0x1) & 3;
                                        device.uuidsByTransport.replace(transUuids[i], new Integer(getTransport));
                                    } else {
                                        device.uuidsByTransport.put(transUuids[i], new Integer(1));
                                    }
                                }

                                if (leTransIndex != -1) {
                                    for (leInx = leTransIndex + 1; leInx < numTransUuids; leInx++) {
                                        if (device.uuidsByTransport.containsKey(transUuids[leInx])) {
                                            int getTransport =
                                                device.uuidsByTransport.get(transUuids[leInx]).intValue();
                                            getTransport = (getTransport |0x2) & 3;
                                            device.uuidsByTransport.replace(transUuids[leInx], new Integer(getTransport));
                                        } else {
                                            device.uuidsByTransport.put(transUuids[leInx], new Integer(2));
                                        }
                                    }
                                }
                            } else {
                                // only LE Transport UUID's
                                if (leTransIndex != -1) {
                                    for (leInx = 1; leInx < numTransUuids; leInx++) {
                                        if (device.uuidsByTransport.containsKey(transUuids[leInx])) {
                                            int getTransport =
                                                device.uuidsByTransport.get(transUuids[leInx]).intValue();
                                            getTransport = (getTransport |0x2) & 3;
                                            device.uuidsByTransport.replace(transUuids[leInx], new Integer(getTransport));
                                        } else {
                                            device.uuidsByTransport.put(transUuids[leInx], new Integer(2));
                                        }
                                    }
                                }
                            }
                        }
                        break;
                        case AbstractionLayer.BT_PROPERTY_REMOTE_IS_COORDINATED_SET_MEMBER:
                            device.mIsCoordinatedSetMember = (boolean) (val[0] != 0);
                            break;
                    }
                }
            }
        }
    }

    void deviceFoundCallback(byte[] address) {
        // The device properties are already registered - we can send the intent
        // now
        BluetoothDevice device = getDevice(address);
        debugLog("deviceFoundCallback: Remote Address is:" + device);
        DeviceProperties deviceProp = getDeviceProperties(device);
        if (deviceProp == null) {
            errorLog("Device Properties is null for Device:" + device);
            return;
        }

        Intent intent = new Intent(BluetoothDevice.ACTION_FOUND);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDevice.EXTRA_CLASS,
                new BluetoothClass(deviceProp.mBluetoothClass));
        intent.putExtra(BluetoothDevice.EXTRA_RSSI, deviceProp.mRssi);
        intent.putExtra(BluetoothDevice.EXTRA_NAME, deviceProp.mName);
        intent.putExtra(BluetoothDevice.EXTRA_IS_COORDINATED_SET_MEMBER,
                deviceProp.mIsCoordinatedSetMember);
        final ArrayList<DiscoveringPackage> packages = sAdapterService.getDiscoveringPackages();
        synchronized (packages) {
            for (DiscoveringPackage pkg : packages) {
                if (pkg.hasDisavowedLocation()) {
                    if (mLocationDenylistPredicate.test(device)) {
                        continue;
                    }
                }

                intent.setPackage(pkg.getPackageName());

                if (pkg.getPermission() != null) {
                    sAdapterService.sendBroadcastMultiplePermissions(intent,
                            new String[] { BLUETOOTH_SCAN, pkg.getPermission() },
                            Utils.getTempAllowlistBroadcastOptions());
                } else {
                    sAdapterService.sendBroadcastMultiplePermissions(intent,
                            new String[] { BLUETOOTH_SCAN },
                            Utils.getTempAllowlistBroadcastOptions());
                }
            }
        }
    }

    void aclStateChangeCallback(int status, byte[] address, int newState,
                                int transportLinkType, int hciReason) {
        BluetoothDevice device = getDevice(address);

        if (device == null) {
            errorLog("aclStateChangeCallback: device is NULL, address="
                    + Utils.getAddressStringFromByte(address) + ", newState=" + newState);
            return;
        }
        int state = sAdapterService.getState();

        Intent intent = null;
        if (newState == AbstractionLayer.BT_ACL_STATE_CONNECTED) {
            if (state == BluetoothAdapter.STATE_ON || state == BluetoothAdapter.STATE_TURNING_ON) {
                intent = new Intent(BluetoothDevice.ACTION_ACL_CONNECTED);
            } else if (state == BluetoothAdapter.STATE_BLE_ON
                    || state == BluetoothAdapter.STATE_BLE_TURNING_ON) {
                intent = new Intent(BluetoothAdapter.ACTION_BLE_ACL_CONNECTED);
            }
            debugLog(
                    "aclStateChangeCallback: Adapter State: " + BluetoothAdapter.nameForState(state)
                            + " Connected: " + device);
        } else {
            if (state == BluetoothAdapter.STATE_ON || state == BluetoothAdapter.STATE_TURNING_OFF) {
                intent = new Intent(BluetoothDevice.ACTION_ACL_DISCONNECTED);
            } else if (state == BluetoothAdapter.STATE_BLE_ON
                    || state == BluetoothAdapter.STATE_BLE_TURNING_OFF) {
                intent = new Intent(BluetoothAdapter.ACTION_BLE_ACL_DISCONNECTED);
            }
            // Reset battery level on complete disconnection
            if (sAdapterService.getConnectionState(device) == 0) {
                resetBatteryLevel(device);
            }
            debugLog(
                    "aclStateChangeCallback: Adapter State: " + BluetoothAdapter.nameForState(state)
                            + " Disconnected: " + device
                            + " transportLinkType: " + transportLinkType
                            + " hciReason: " + hciReason);
        }

        int connectionState = newState == AbstractionLayer.BT_ACL_STATE_CONNECTED
                ? BluetoothAdapter.STATE_CONNECTED : BluetoothAdapter.STATE_DISCONNECTED;
        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_ACL_CONNECTION_STATE_CHANGED,
                sAdapterService.obfuscateAddress(device), connectionState, 0);
        BluetoothClass deviceClass = device.getBluetoothClass();
        int classOfDevice = deviceClass == null ? 0 : deviceClass.getClassOfDevice();
        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_CLASS_OF_DEVICE_REPORTED,
                sAdapterService.obfuscateAddress(device), classOfDevice, 0);

        if (intent != null) {
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                    | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
            sAdapterService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                    Utils.getTempAllowlistBroadcastOptions());

            synchronized (sAdapterService.getBluetoothConnectionCallbacks()) {
                Set<IBluetoothConnectionCallback> bluetoothConnectionCallbacks =
                        sAdapterService.getBluetoothConnectionCallbacks();
                for (IBluetoothConnectionCallback callback : bluetoothConnectionCallbacks) {
                    try {
                        if (connectionState == BluetoothAdapter.STATE_CONNECTED) {
                            callback.onDeviceConnected(device);
                        } else {
                            callback.onDeviceDisconnected(device,
                                AdapterService.hciToAndroidDisconnectReason(hciReason));
                        }
                    } catch (RemoteException ex) {
                        Log.e(TAG, "RemoteException in calling IBluetoothConnectionCallback");
                    }
                }
            }
        } else {
            Log.e(TAG, "aclStateChangeCallback intent is null. deviceBondState: "
                    + device.getBondState());
        }
    }


    void fetchUuids(BluetoothDevice device) {
        if (sSdpTracker.contains(device)) {
            return;
        }
        sSdpTracker.add(device);

        Message message = mHandler.obtainMessage(MESSAGE_UUID_INTENT);
        message.obj = device;
        mHandler.sendMessageDelayed(message, UUID_INTENT_DELAY);

        sAdapterService.getRemoteServicesNative(Utils.getBytesFromAddress(device.getAddress()));
    }

    void updateUuids(BluetoothDevice device) {
        Message message = mHandler.obtainMessage(MESSAGE_UUID_INTENT);
        message.obj = device;
        mHandler.sendMessage(message);
    }

    /**
     * Handles headset connection state change event
     * @param intent must be {@link BluetoothHeadset#ACTION_CONNECTION_STATE_CHANGED} intent
     */
    @VisibleForTesting
    void onHeadsetConnectionStateChanged(Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        if (device == null) {
            Log.e(TAG, "onHeadsetConnectionStateChanged() remote device is null");
            return;
        }
        if (intent.getIntExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_DISCONNECTED)
                == BluetoothProfile.STATE_DISCONNECTED) {
            // TODO: Rework this when non-HFP sources of battery level indication is added
            resetBatteryLevel(device);
        }
    }

    @VisibleForTesting
    void onHfIndicatorValueChanged(Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        if (device == null) {
            Log.e(TAG, "onHfIndicatorValueChanged() remote device is null");
            return;
        }
        int indicatorId = intent.getIntExtra(BluetoothHeadset.EXTRA_HF_INDICATORS_IND_ID, -1);
        int indicatorValue = intent.getIntExtra(BluetoothHeadset.EXTRA_HF_INDICATORS_IND_VALUE, -1);
        if (indicatorId == HeadsetHalConstants.HF_INDICATOR_BATTERY_LEVEL_STATUS) {
            updateBatteryLevel(device, indicatorValue);
        }
    }

    /**
     * Handle {@link BluetoothHeadset#ACTION_VENDOR_SPECIFIC_HEADSET_EVENT} intent
     * @param intent must be {@link BluetoothHeadset#ACTION_VENDOR_SPECIFIC_HEADSET_EVENT} intent
     */
    @VisibleForTesting
    void onVendorSpecificHeadsetEvent(Intent intent) {
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        if (device == null) {
            Log.e(TAG, "onVendorSpecificHeadsetEvent() remote device is null");
            return;
        }
        String cmd =
                intent.getStringExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD);
        if (cmd == null) {
            Log.e(TAG, "onVendorSpecificHeadsetEvent() command is null");
            return;
        }
        int cmdType =
                intent.getIntExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD_TYPE,
                        -1);
        // Only process set command
        if (cmdType != BluetoothHeadset.AT_CMD_TYPE_SET) {
            debugLog("onVendorSpecificHeadsetEvent() only SET command is processed");
            return;
        }
        Object[] args = (Object[]) intent.getExtras()
                .get(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_ARGS);
        if (args == null) {
            Log.e(TAG, "onVendorSpecificHeadsetEvent() arguments are null");
            return;
        }
        int batteryPercent = BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        switch (cmd) {
            case BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XEVENT:
                batteryPercent = getBatteryLevelFromXEventVsc(args);
                break;
            case BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_IPHONEACCEV:
                batteryPercent = getBatteryLevelFromAppleBatteryVsc(args);
                break;
        }
        if (batteryPercent != BluetoothDevice.BATTERY_LEVEL_UNKNOWN) {
            updateBatteryLevel(device, batteryPercent);
            infoLog("Updated device " + device + " battery level to " + String.valueOf(
                    batteryPercent) + "%");
        }
    }

    /**
     * Parse
     *      AT+IPHONEACCEV=[NumberOfIndicators],[IndicatorType],[IndicatorValue]
     * vendor specific event
     * @param args Array of arguments on the right side of assignment
     * @return Battery level in percents, [0-100], {@link BluetoothDevice#BATTERY_LEVEL_UNKNOWN}
     *         when there is an error parsing the arguments
     */
    @VisibleForTesting
    static int getBatteryLevelFromAppleBatteryVsc(Object[] args) {
        if (args.length == 0) {
            Log.w(TAG, "getBatteryLevelFromAppleBatteryVsc() empty arguments");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        int numKvPair;
        if (args[0] instanceof Integer) {
            numKvPair = (Integer) args[0];
        } else {
            Log.w(TAG, "getBatteryLevelFromAppleBatteryVsc() error parsing number of arguments");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        if (args.length != (numKvPair * 2 + 1)) {
            Log.w(TAG, "getBatteryLevelFromAppleBatteryVsc() number of arguments does not match");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        int indicatorType;
        int indicatorValue = -1;
        for (int i = 0; i < numKvPair; ++i) {
            Object indicatorTypeObj = args[2 * i + 1];
            if (indicatorTypeObj instanceof Integer) {
                indicatorType = (Integer) indicatorTypeObj;
            } else {
                Log.w(TAG, "getBatteryLevelFromAppleBatteryVsc() error parsing indicator type");
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }
            if (indicatorType
                    != BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_IPHONEACCEV_BATTERY_LEVEL) {
                continue;
            }
            Object indicatorValueObj = args[2 * i + 2];
            if (indicatorValueObj instanceof Integer) {
                indicatorValue = (Integer) indicatorValueObj;
            } else {
                Log.w(TAG, "getBatteryLevelFromAppleBatteryVsc() error parsing indicator value");
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }
            break;
        }
        return (indicatorValue < 0 || indicatorValue > 9) ? BluetoothDevice.BATTERY_LEVEL_UNKNOWN
                : (indicatorValue + 1) * 10;
    }

    /**
     * Parse
     *      AT+XEVENT=BATTERY,[Level],[NumberOfLevel],[MinutesOfTalk],[IsCharging]
     * vendor specific event
     * @param args Array of arguments on the right side of SET command
     * @return Battery level in percents, [0-100], {@link BluetoothDevice#BATTERY_LEVEL_UNKNOWN}
     *         when there is an error parsing the arguments
     */
    @VisibleForTesting
    static int getBatteryLevelFromXEventVsc(Object[] args) {
        if (args.length == 0) {
            Log.w(TAG, "getBatteryLevelFromXEventVsc() empty arguments");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        Object eventNameObj = args[0];
        if (!(eventNameObj instanceof String)) {
            Log.w(TAG, "getBatteryLevelFromXEventVsc() error parsing event name");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        String eventName = (String) eventNameObj;
        if (!eventName.equals(
                BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XEVENT_BATTERY_LEVEL)) {
            infoLog("getBatteryLevelFromXEventVsc() skip none BATTERY event: " + eventName);
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        if (args.length != 5) {
            Log.w(TAG, "getBatteryLevelFromXEventVsc() wrong battery level event length: "
                    + String.valueOf(args.length));
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        if (!(args[1] instanceof Integer) || !(args[2] instanceof Integer)) {
            Log.w(TAG, "getBatteryLevelFromXEventVsc() error parsing event values");
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        int batteryLevel = (Integer) args[1];
        int numberOfLevels = (Integer) args[2];
        if (batteryLevel < 0 || numberOfLevels <= 1 || batteryLevel > numberOfLevels) {
            Log.w(TAG, "getBatteryLevelFromXEventVsc() wrong event value, batteryLevel="
                    + String.valueOf(batteryLevel) + ", numberOfLevels=" + String.valueOf(
                    numberOfLevels));
            return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
        }
        return batteryLevel * 100 / (numberOfLevels - 1);
    }

    private static void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    private static void debugLog(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    private static void infoLog(String msg) {
        if (DBG) {
            Log.i(TAG, msg);
        }
    }

    private static void warnLog(String msg) {
        Log.w(TAG, msg);
    }

}
