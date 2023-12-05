/*
 * Copyright (C) 2018 The Linux Foundation. All rights reserved.
 * Not a Contribution
 * Copyright (C) 2012 The Android Open Source Project
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

import android.annotation.RequiresPermission;
import android.annotation.Nullable;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProtoEnums;
import android.bluetooth.OobData;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.UserHandle;
import android.os.SystemProperties;
import android.util.Log;
import android.os.PowerManager;
import com.android.bluetooth.BluetoothStatsLog;

import com.android.bluetooth.Utils;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.btservice.RemoteDevices.DeviceProperties;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hfpclient.HeadsetClientService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.pbapclient.PbapClientService;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;


/**
 * This state machine handles Bluetooth Adapter State.
 * States:
 *      {@link StableState} :  No device is in bonding / unbonding state.
 *      {@link PendingCommandState} : Some device is in bonding / unbonding state.
 * TODO(BT) This class can be removed and this logic moved to the stack.
 */

final class BondStateMachine extends StateMachine {
    private static final boolean DBG = false;
    private static final String TAG = "BluetoothBondStateMachine";

    static final int CREATE_BOND = 1;
    static final int CANCEL_BOND = 2;
    static final int REMOVE_BOND = 3;
    static final int BONDING_STATE_CHANGE = 4;
    static final int SSP_REQUEST = 5;
    static final int PIN_REQUEST = 6;
    static final int UUID_UPDATE = 10;
    static final int BOND_STATE_NONE = 0;
    static final int BOND_STATE_BONDING = 1;
    static final int BOND_STATE_BONDED = 2;
    static final int ADD_DEVICE_BOND_QUEUE = 11;
    private static final int GROUP_ID_START = 0;
    private static final int GROUP_ID_END = 15;

    private AdapterService mAdapterService;
    private AdapterProperties mAdapterProperties;
    private RemoteDevices mRemoteDevices;
    private BluetoothAdapter mAdapter;

    /* The WakeLock is used for bringing up the LCD during a pairing request
     * from remote device when Android is in Suspend state.*/
    private PowerManager.WakeLock mWakeLock;

    private PendingCommandState mPendingCommandState = new PendingCommandState();
    private StableState mStableState = new StableState();

    public static final String OOBDATAP192 = "oobdatap192";
    public static final String OOBDATAP256 = "oobdatap256";

    @VisibleForTesting Set<BluetoothDevice> mPendingBondedDevices = new HashSet<>();

    private final ArrayList<BluetoothDevice> mDevices =
        new ArrayList<BluetoothDevice>();

    private final HashMap<BluetoothDevice, Integer> mBondingQueue
        = new HashMap<BluetoothDevice, Integer>();

    private final HashMap<BluetoothDevice, Integer> mBondingDevStatus
        = new HashMap<BluetoothDevice, Integer>();

    private BondStateMachine(PowerManager pm, AdapterService service,
            AdapterProperties prop, RemoteDevices remoteDevices) {
        super("BondStateMachine:");
        addState(mStableState);
        addState(mPendingCommandState);
        mRemoteDevices = remoteDevices;
        mAdapterService = service;
        mAdapterProperties = prop;
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        setInitialState(mStableState);

        //WakeLock instantiation in RemoteDevices class
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK
                | PowerManager.ACQUIRE_CAUSES_WAKEUP | PowerManager.ON_AFTER_RELEASE, TAG);
        mWakeLock.setReferenceCounted(false);
    }

    public static BondStateMachine make(PowerManager pm, AdapterService service,
            AdapterProperties prop, RemoteDevices remoteDevices) {
        Log.d(TAG, "make");
        BondStateMachine bsm = new BondStateMachine(pm, service, prop, remoteDevices);
        bsm.start();
        return bsm;
    }

    public synchronized void doQuit() {
        quitNow();
    }

    private void cleanup() {
        mAdapterService = null;
        mRemoteDevices = null;
        mAdapterProperties = null;
    }

    @Override
    protected void onQuitting() {
        cleanup();
    }

    private class StableState extends State {
        @Override
        public void enter() {
            infoLog("StableState(): Entering Off State");
        }

        @Override
        public synchronized boolean processMessage(Message msg) {

            BluetoothDevice dev = (BluetoothDevice) msg.obj;

            switch (msg.what) {

                case CREATE_BOND:
                    OobData p192Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP192) : null;
                    OobData p256Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP256) : null;
                    createBond(dev, msg.arg1, p192Data, p256Data, true);
                    break;
                case REMOVE_BOND:
                    removeBond(dev, true);
                    break;
                case BONDING_STATE_CHANGE:
                    int newState = msg.arg1;
                    /* if incoming pairing, transition to pending state */
                    if (newState == BluetoothDevice.BOND_BONDING) {
                        if (!mDevices.contains(dev)) {
                            mDevices.add(dev);
                        }
                        sendIntent(dev, newState, 0);
                        transitionTo(mPendingCommandState);
                    } else if (newState == BluetoothDevice.BOND_NONE) {
                        /* if the link key was deleted by the stack */
                        sendIntent(dev, newState, 0);
                    } else {
                        Log.e(TAG, "In stable state, received invalid newState: "
                                + state2str(newState));
                    }
                    break;
                 case ADD_DEVICE_BOND_QUEUE:
                    int groupIdentifer = msg.arg1;
                    OobData m192Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP192) : null;
                    OobData m256Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP256) : null;
                    Log.i(TAG, "Adding to bonding queue in stableState, " + dev +
                        ", mDevices.size()=" + mDevices.size() +
                        ", mPendingBondedDevices.size()=" + mPendingBondedDevices.size());
                    Integer groupId = new Integer(groupIdentifer);
                    mBondingQueue.put(dev , groupId);
                    mBondingDevStatus.put(dev, 0);

                    if (mDevices.size() == 0 && mPendingBondedDevices.isEmpty()) {
                        if (mAdapterService.isSdpCompleted(dev)) {
                            boolean status = createBond(dev, 0, m192Data, m256Data, true);
                            if (status)
                                mBondingDevStatus.put(dev, 1);
                        }
                    }

                    break;
                case UUID_UPDATE:
                    if (mPendingBondedDevices.contains(dev)) {
                        sendIntent(dev, BluetoothDevice.BOND_BONDED, 0);
                    }
                    break;
                case CANCEL_BOND:
                default:
                    Log.e(TAG, "Received unhandled state: " + msg.what);
                    return false;
            }
            return true;
        }
    }


    private class PendingCommandState extends State {

        @Override
        public void enter() {
            infoLog("Entering PendingCommandState State");
            BluetoothDevice dev = (BluetoothDevice) getCurrentMessage().obj;
        }

        @Override
        public synchronized boolean processMessage(Message msg) {
            BluetoothDevice dev = (BluetoothDevice) msg.obj;
            DeviceProperties devProp = mRemoteDevices.getDeviceProperties(dev);
            boolean result = false;
            if (mDevices.contains(dev) && msg.what != CANCEL_BOND
                    && msg.what != BONDING_STATE_CHANGE && msg.what != SSP_REQUEST
                    && msg.what != PIN_REQUEST) {
                deferMessage(msg);
                return true;
            }

            switch (msg.what) {
                case CREATE_BOND:
                    OobData p192Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP192) : null;
                    OobData p256Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP256) : null;
                    result = createBond(dev, msg.arg1, p192Data, p256Data, false);
                    break;
                case REMOVE_BOND:
                    result = removeBond(dev, false);
                    break;
                case CANCEL_BOND:
                    result = cancelBond(dev);
                    break;
                case BONDING_STATE_CHANGE:
                    int newState = msg.arg1;
                    int reason = getUnbondReasonFromHALCode(msg.arg2);
                    // Bond is explicitly removed if we are in pending command state
                    if (newState == BluetoothDevice.BOND_NONE
                            && reason == BluetoothDevice.BOND_SUCCESS) {
                        reason = BluetoothDevice.UNBOND_REASON_REMOVED;
                    }
                    sendIntent(dev, newState, reason);
                    if (newState != BluetoothDevice.BOND_BONDING) {
                        // check if bond none is received from device which
                        // was in pairing state otherwise don't transition to
                        // stable state.
                        if (newState == BluetoothDevice.BOND_NONE &&
                            !mDevices.contains(dev) && mDevices.size() != 0) {
                            infoLog("not transitioning to stable state");
                            break;
                        }
                        // This is either none/bonded, remove and transition, and also set
                        // result=false to avoid adding the device to mDevices.
                        if (mDevices.contains(dev)) {
                            mDevices.remove(dev);
                        } else  {
                            Log.w(TAG,"device already removed from mDevices");
                        }
                        result = false;
                        if (mDevices.isEmpty()) {
                            transitionTo(mStableState);
                        }
                        if (newState == BluetoothDevice.BOND_NONE) {
                            mAdapterService.setPhonebookAccessPermission(dev,
                                    BluetoothDevice.ACCESS_UNKNOWN);
                            mAdapterService.setMessageAccessPermission(dev,
                                    BluetoothDevice.ACCESS_UNKNOWN);
                            mAdapterService.setSimAccessPermission(dev,
                                    BluetoothDevice.ACCESS_UNKNOWN);
                            // Set the profile Priorities to undefined
                            clearProfilePriority(dev);
                        }
                    } else if (!mDevices.contains(dev)) {
                        result = true;
                    }
                    break;
                case SSP_REQUEST:
                    int passkey = msg.arg1;
                    int variant = msg.arg2;
                    if (devProp == null)
                    {
                        Log.e(TAG,"Received msg from an unknown device");
                        return false;
                    }
                    sendDisplayPinIntent(devProp.getAddress(), passkey, variant);
                    if(SystemProperties.get("ro.board.platform").equals("neo")) {
                        Log.d(TAG,"Auto Accept pairing request for Neo devices");
                        BluetoothDevice device = (BluetoothDevice)msg.obj;
                        device.setPairingConfirmation(true);
                    }
                    break;
                case PIN_REQUEST:
                    BluetoothClass btClass = dev.getBluetoothClass();
                    int new_state = mAdapterService.getState();
                    if (new_state != BluetoothAdapter.STATE_ON) return false;
                    int btDeviceClass = btClass.getDeviceClass();
                    if (devProp == null)
                    {
                        Log.e(TAG,"Received msg from an unknown device");
                        return false;
                    }
                    if (btDeviceClass == BluetoothClass.Device.PERIPHERAL_KEYBOARD || btDeviceClass
                            == BluetoothClass.Device.PERIPHERAL_KEYBOARD_POINTING) {
                        // Its a keyboard. Follow the HID spec recommendation of creating the
                        // passkey and displaying it to the user. If the keyboard doesn't follow
                        // the spec recommendation, check if the keyboard has a fixed PIN zero
                        // and pair.
                        //TODO: Maintain list of devices that have fixed pin
                        // Generate a variable 6-digit PIN in range of 100000-999999
                        // This is not truly random but good enough.
                        int pin = 100000 + (int) Math.floor((Math.random() * (999999 - 100000)));
                        sendDisplayPinIntent(devProp.getAddress(), pin,
                                BluetoothDevice.PAIRING_VARIANT_DISPLAY_PIN);
                        break;
                    }

                    if (msg.arg2 == 1) { // Minimum 16 digit pin required here
                        sendDisplayPinIntent(devProp.getAddress(), 0,
                                BluetoothDevice.PAIRING_VARIANT_PIN_16_DIGITS);
                    } else {
                        // In PIN_REQUEST, there is no passkey to display.So do not send the
                        // EXTRA_PAIRING_KEY type in the intent( 0 in SendDisplayPinIntent() )
                        sendDisplayPinIntent(devProp.getAddress(), 0,
                                BluetoothDevice.PAIRING_VARIANT_PIN);
                    }

                    break;
                case ADD_DEVICE_BOND_QUEUE:
                    int groupIdentifer = msg.arg1;
                    OobData m192Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP192) : null;
                    OobData m256Data = (msg.getData() != null)
                            ? msg.getData().getParcelable(OOBDATAP256) : null;
                    Log.i(TAG, "Adding to bonding queue in pendingState " + dev +
                        ", mDevices.size()=" + mDevices.size() +
                        ", mPendingBondedDevices.size()=" + mPendingBondedDevices.size());
                    Integer groupId = new Integer(groupIdentifer);
                    //mAdapterProperties.onBondStateChanged(dev, BluetoothDevice.BOND_NONE);
                    mBondingQueue.put(dev , groupId);
                    mBondingDevStatus.put(dev, 0);

                    if (mDevices.size() == 0 && mPendingBondedDevices.isEmpty()) {
                        if (mAdapterService.isSdpCompleted(dev)) {
                            boolean status = createBond(dev, 0, m192Data, m256Data, true);
                            if (status)
                                mBondingDevStatus.put(dev, 1);
                        }
                    }

                    break;
                default:
                    Log.e(TAG, "Received unhandled event:" + msg.what);
                    return false;
            }
            if (result) {
                Log.i(TAG, "Adding to Device Queue" +dev.getAddress());
                mDevices.add(dev);
            }

            return true;
        }
    }

    private boolean cancelBond(BluetoothDevice dev) {
        if (mAdapterService == null) return false;
        if (dev.getBondState() == BluetoothDevice.BOND_BONDING) {
            byte[] addr = Utils.getBytesFromAddress(dev.getAddress());
            if (!mAdapterService.cancelBondNative(addr)) {
                Log.e(TAG, "Unexpected error while cancelling bond:");
            } else {
                return true;
            }
        }
        return false;
    }

    private boolean removeBond(BluetoothDevice dev, boolean transition) {
        if (mAdapterService == null) return false;
        DeviceProperties devProp = mRemoteDevices.getDeviceProperties(dev);
        if (devProp != null && devProp.getBondState() == BluetoothDevice.BOND_BONDED) {
            byte[] addr = Utils.getBytesFromAddress(dev.getAddress());
            if (!mAdapterService.removeBondNative(addr)) {
                Log.e(TAG, "Unexpected error while removing bond:");
            } else {
                if (transition) {
                    transitionTo(mPendingCommandState);
                }
                return true;
            }

        }
        return false;
    }

    private boolean createBond(BluetoothDevice dev, int transport, OobData remoteP192Data,
            OobData remoteP256Data, boolean transition) {
        if (mAdapterService == null) return false;
        if (dev.getBondState() == BluetoothDevice.BOND_NONE) {
            infoLog("Bond address is:" + dev);
            byte[] addr = Utils.getBytesFromAddress(dev.getAddress());
            boolean result;
            if (mAdapterService.isAdvAudioDevice(dev)) {
                infoLog("createBond for ADV AUDIO DEVICE going through Transport " + dev);
            }
            // If we have some data
            if (remoteP192Data != null || remoteP256Data != null) {
                result = mAdapterService.createBondOutOfBandNative(addr, transport,
                    remoteP192Data, remoteP256Data);
            } else {
                result = mAdapterService.createBondNative(addr, transport);
            }
            BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                    mAdapterService.obfuscateAddress(dev), transport, dev.getType(),
                    BluetoothDevice.BOND_BONDING,
                    remoteP192Data == null && remoteP256Data == null
                            ? BluetoothProtoEnums.BOND_SUB_STATE_UNKNOWN
                            : BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_OOB_DATA_PROVIDED,
                    BluetoothProtoEnums.UNBOND_REASON_UNKNOWN);

            if (!result) {
                BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                        mAdapterService.obfuscateAddress(dev), transport, dev.getType(),
                        BluetoothDevice.BOND_NONE, BluetoothProtoEnums.BOND_SUB_STATE_UNKNOWN,
                        BluetoothDevice.UNBOND_REASON_REPEATED_ATTEMPTS);
                // Using UNBOND_REASON_REMOVED for legacy reason
                sendIntent(dev, BluetoothDevice.BOND_NONE, BluetoothDevice.UNBOND_REASON_REMOVED);
                return false;
            } else if (transition) {
                transitionTo(mPendingCommandState);
            }
            return true;
        }
        return false;
    }

    private void sendDisplayPinIntent(byte[] address, int pin, int variant) {

        // Acquire wakelock during PIN code request to bring up LCD display
        mWakeLock.acquire();
        Intent intent = new Intent(BluetoothDevice.ACTION_PAIRING_REQUEST);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mRemoteDevices.getDevice(address));
        if (pin != 0) {
            intent.putExtra(BluetoothDevice.EXTRA_PAIRING_KEY, pin);
        }
        intent.putExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT, variant);
        intent.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        // Workaround for Android Auto until pre-accepting pairing requests is added.
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mAdapterService.sendOrderedBroadcast(intent, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions(), null/* resultReceiver */,
                null/* scheduler */, Activity.RESULT_OK/* initialCode */, null/* initialData */,
                null/* initialExtras */);
        // Release wakelock to allow the LCD to go off after the PIN popup notification.
        mWakeLock.release();
    }

    private BluetoothDevice getNextBondingGroupDevice() {

        Iterator<Map.Entry<BluetoothDevice, Integer>> tmpItr
            = mBondingDevStatus.entrySet().iterator();

        while (tmpItr.hasNext()) {
            Map.Entry<BluetoothDevice, Integer> groupMember
                = (Map.Entry)tmpItr.next();
            if (groupMember != null) {
                BluetoothDevice device = groupMember.getKey();
                int bondStatus = groupMember.getValue().intValue();
                if (bondStatus == 0) {
                  infoLog("Found device with status 0 " + device.getAddress());
                  return device;
                }
            }
        }
        return null;
    }

    private boolean isAdvAudioDevice(BluetoothDevice device) {
        if (mAdapterService.isAdvAudioDevice(device))
            return true;

        if(mBondingQueue.containsKey(device)) {
            infoLog("Found isAdvAudioDevice in Bonding Queue "
                + device.getAddress());
            return true;
        }

        return false;
    }

    @VisibleForTesting
    void sendIntent(BluetoothDevice device, int newState, int reason) {
        DeviceProperties devProp = mRemoteDevices.getDeviceProperties(device);
        int oldState = BluetoothDevice.BOND_NONE;
        if (newState != BluetoothDevice.BOND_NONE
                && newState != BluetoothDevice.BOND_BONDING
                && newState != BluetoothDevice.BOND_BONDED) {
            infoLog("Invalid bond state " + newState);
            return;
        }
        if (devProp != null) {
            oldState = devProp.getBondState();
        }
        if (mPendingBondedDevices.contains(device)) {
            mPendingBondedDevices.remove(device);
            if (oldState == BluetoothDevice.BOND_BONDED) {
                if (newState == BluetoothDevice.BOND_BONDING) {
                    mAdapterProperties.onBondStateChanged(device, newState);
                }
                oldState = BluetoothDevice.BOND_BONDING;
            } else {
                // Should not enter here.
                throw new IllegalArgumentException("Invalid old state " + oldState);
            }
        }
        if (oldState == newState) {
            return;
        }

        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                mAdapterService.obfuscateAddress(device), 0, device.getType(),
                newState, BluetoothProtoEnums.BOND_SUB_STATE_UNKNOWN, reason);
        BluetoothClass deviceClass = device.getBluetoothClass();
        int classOfDevice = deviceClass == null ? 0 : deviceClass.getClassOfDevice();
        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_CLASS_OF_DEVICE_REPORTED,
                mAdapterService.obfuscateAddress(device), classOfDevice, 0);

        mAdapterProperties.onBondStateChanged(device, newState);

        if (devProp != null && (((devProp.getDeviceType() == BluetoothDevice.DEVICE_TYPE_CLASSIC
                || devProp.getDeviceType() == BluetoothDevice.DEVICE_TYPE_DUAL) ||
                (isAdvAudioDevice(device)))
                && newState == BluetoothDevice.BOND_BONDED && devProp.getUuids() == null)) {
            infoLog(device + " is bonded, wait for SDP complete to broadcast bonded intent");
            if (!mPendingBondedDevices.contains(device)) {
                mPendingBondedDevices.add(device);
            }
            if (oldState == BluetoothDevice.BOND_NONE) {
                // Broadcast NONE->BONDING for NONE->BONDED case.
                newState = BluetoothDevice.BOND_BONDING;
            } else {
                if (newState == BluetoothDevice.BOND_BONDED ) {
                    mAdapterProperties.updateSdpProgress(device, false /*SDP pending*/);
                }
                return;
            }
        }

        if ((newState == BluetoothDevice.BOND_BONDED )
           ||(newState == BluetoothDevice.BOND_NONE)){
            mAdapterProperties.updateSdpProgress(device, true /* SDP Completed */);
        }
        Intent intent = new Intent(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDevice.EXTRA_BOND_STATE, newState);
        intent.putExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, oldState);
        if (newState == BluetoothDevice.BOND_NONE) {
            intent.putExtra(BluetoothDevice.EXTRA_REASON, reason);
        }
        if (newState == BluetoothDevice.BOND_BONDED) {
            int validAddr = devProp.getValidBDAddr();
            if (validAddr == 0) {
                intent.putExtra(BluetoothDevice.EXTRA_IS_PRIVATE_ADDRESS, true);
            }
            infoLog("SEND INTENT of " + device + "with validAddr " + validAddr);
        }

        if (mAdapterService.isAdvAudioDevice(device) &&
            (newState == BluetoothDevice.BOND_BONDED)) {
            if (mAdapterService.isGroupDevice(device)) {
                int groupId = mAdapterService.getGroupId(device);
                if ((groupId >= GROUP_ID_START) && groupId <= GROUP_ID_END) {
                    infoLog("SEND INTENT of " + device +
                        " with groupId " + groupId);
                    intent.putExtra(BluetoothDevice.EXTRA_GROUP_ID, groupId);
                }
            } else {
                BluetoothDevice mapBdAddr =
                    mAdapterService.getIdentityAddress(device);
                if (mapBdAddr != null) {
                    if  (mBondingQueue.containsKey(mapBdAddr)) {
                        infoLog(" Mapped Address in the queue" + mapBdAddr);
                        Integer groupid = mBondingQueue.get(mapBdAddr);
                        if ((groupid != null)
                            && ((groupid.intValue() >= 0)  && ((groupid.intValue() <= 15)))) {
                            infoLog("Device in Bonding queue" + device
                                + " with groupid " + groupid);
                            intent.putExtra(BluetoothDevice.EXTRA_GROUP_ID, groupid);
                        }
                    }
                }
            }
        }
        mAdapterService.sendBroadcastAsUser(intent, UserHandle.ALL, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
        infoLog("Bond State Change Intent:" + device + " " + state2str(oldState) + " => "
                + state2str(newState));

        //TODO Move below code separate API
        if (newState == BluetoothDevice.BOND_NONE ||
            newState == BluetoothDevice.BOND_BONDED) {
            if(mBondingQueue.containsKey(device)) {
                infoLog("Removing Device from Bonding Queue");
                mBondingQueue.remove(device);
                mBondingDevStatus.remove(device);
            }
            infoLog("Bonded Completed " + device);
            infoLog("mBondingDevStatus size " + mBondingDevStatus.size());
            //TODO Move below code to separate API
            if (mBondingDevStatus.size() > 0) {
                BluetoothDevice dev =  getNextBondingGroupDevice();
                infoLog("Try to bond next device in queue " + dev);
                if (dev != null) {
                    if (createBond(dev, 0, null, null, true)) {
                        infoLog("Bonding next Device from Bonding Queue"
                            + dev.getAddress());
                        mBondingDevStatus.put(dev, 1);
                    } else {
                        infoLog("Failed Retry Next time "
                            +dev.getAddress() + " bond state " + dev.getBondState());
                    }
                }
            }
        }
    }

    void bondStateChangeCallback(int status, byte[] address, int newState, int hciReason) {
        BluetoothDevice device = mRemoteDevices.getDevice(address);

        if (device == null) {
            infoLog("No record of the device:" + device);
            // This device will be added as part of the BONDING_STATE_CHANGE intent processing
            // in sendIntent above
            device = mAdapter.getRemoteDevice(Utils.getAddressStringFromByte(address));
        }

        infoLog("bondStateChangeCallback: Status: " + status + " Address: " + device + " newState: "
                + newState + " hciReason: " + hciReason);

        Message msg = obtainMessage(BONDING_STATE_CHANGE);
        msg.obj = device;

        if (newState == BOND_STATE_BONDED) {
            msg.arg1 = BluetoothDevice.BOND_BONDED;
        } else if (newState == BOND_STATE_BONDING) {
            msg.arg1 = BluetoothDevice.BOND_BONDING;
        } else {
            msg.arg1 = BluetoothDevice.BOND_NONE;
        }
        msg.arg2 = status;

        sendMessage(msg);
    }

    void sspRequestCallback(byte[] address, byte[] name, int cod, int pairingVariant, int passkey) {
        //TODO(BT): Get wakelock and update name and cod
        BluetoothDevice bdDevice = mRemoteDevices.getDevice(address);
        if (bdDevice == null) {
            mRemoteDevices.addDeviceProperties(address);
        }
        infoLog("sspRequestCallback: " + address + " name: " + name + " cod: " + cod
                + " pairingVariant " + pairingVariant + " passkey: "
                + (Build.isDebuggable() ? passkey : "******"));
        int variant;
        boolean displayPasskey = false;
        switch (pairingVariant) {

            case AbstractionLayer.BT_SSP_VARIANT_PASSKEY_CONFIRMATION:
                variant = BluetoothDevice.PAIRING_VARIANT_PASSKEY_CONFIRMATION;
                displayPasskey = true;
                break;

            case AbstractionLayer.BT_SSP_VARIANT_CONSENT:
                variant = BluetoothDevice.PAIRING_VARIANT_CONSENT;
                break;

            case AbstractionLayer.BT_SSP_VARIANT_PASSKEY_ENTRY:
                variant = BluetoothDevice.PAIRING_VARIANT_PASSKEY;
                break;

            case AbstractionLayer.BT_SSP_VARIANT_PASSKEY_NOTIFICATION:
                variant = BluetoothDevice.PAIRING_VARIANT_DISPLAY_PASSKEY;
                displayPasskey = true;
                break;

            default:
                errorLog("SSP Pairing variant not present");
                return;
        }
        BluetoothDevice device = mRemoteDevices.getDevice(address);
        if (device == null) {
            warnLog("Device is not known for:" + Utils.getAddressStringFromByte(address));
            mRemoteDevices.addDeviceProperties(address);
            device = Objects.requireNonNull(mRemoteDevices.getDevice(address));
        }

        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                mAdapterService.obfuscateAddress(device), 0, device.getType(),
                BluetoothDevice.BOND_BONDING,
                BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_SSP_REQUESTED, 0);

        Message msg = obtainMessage(SSP_REQUEST);
        msg.obj = device;
        if (displayPasskey) {
            msg.arg1 = passkey;
        }
        msg.arg2 = variant;
        sendMessage(msg);
    }

    void pinRequestCallback(byte[] address, byte[] name, int cod, boolean min16Digits) {
        //TODO(BT): Get wakelock and update name and cod

        BluetoothDevice bdDevice = mRemoteDevices.getDevice(address);
        if (bdDevice == null) {
            mRemoteDevices.addDeviceProperties(address);
            bdDevice = Objects.requireNonNull(mRemoteDevices.getDevice(address));
        }

        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                mAdapterService.obfuscateAddress(bdDevice), 0, bdDevice.getType(),
                BluetoothDevice.BOND_BONDING,
                BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_PIN_REQUESTED, 0);

        infoLog("pinRequestCallback: " + address + " name:" + name + " cod:" + cod);

        Message msg = obtainMessage(PIN_REQUEST);
        msg.obj = bdDevice;
        msg.arg2 = min16Digits ? 1 : 0; // Use arg2 to pass the min16Digit boolean

        sendMessage(msg);
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
            android.Manifest.permission.MODIFY_PHONE_STATE,
    })
    private void clearProfilePriority(BluetoothDevice device) {
        HidHostService hidService = HidHostService.getHidHostService();
        A2dpService a2dpService = A2dpService.getA2dpService();
        HeadsetService headsetService = HeadsetService.getHeadsetService();
        HeadsetClientService headsetClientService = HeadsetClientService.getHeadsetClientService();
        A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
        PbapClientService pbapClientService = PbapClientService.getPbapClientService();

        if (hidService != null) {
            hidService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }
        if (a2dpService != null) {
            a2dpService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }
        if (headsetService != null) {
            headsetService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }
        if (headsetClientService != null) {
            headsetClientService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }
        if (a2dpSinkService != null) {
            a2dpSinkService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }
        if (pbapClientService != null) {
            pbapClientService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
        }

        // Clear Absolute Volume black list
        if (a2dpService != null) {
            a2dpService.resetAvrcpBlacklist(device);
        }
    }

    private String state2str(int state) {
        if (state == BluetoothDevice.BOND_NONE) {
            return "BOND_NONE";
        } else if (state == BluetoothDevice.BOND_BONDING) {
            return "BOND_BONDING";
        } else if (state == BluetoothDevice.BOND_BONDED) {
            return "BOND_BONDED";
        } else return "UNKNOWN(" + state + ")";
    }

    private void infoLog(String msg) {
        Log.i(TAG, msg);
    }

    private void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    private void warnLog(String msg) {
        Log.w(TAG, msg);
    }

    private int getUnbondReasonFromHALCode(int reason) {
        if (reason == AbstractionLayer.BT_STATUS_SUCCESS) {
            return BluetoothDevice.BOND_SUCCESS;
        } else if (reason == AbstractionLayer.BT_STATUS_RMT_DEV_DOWN) {
            return BluetoothDevice.UNBOND_REASON_REMOTE_DEVICE_DOWN;
        } else if (reason == AbstractionLayer.BT_STATUS_AUTH_FAILURE) {
            return BluetoothDevice.UNBOND_REASON_AUTH_FAILED;
        } else if (reason == AbstractionLayer.BT_STATUS_AUTH_REJECTED) {
            return BluetoothDevice.UNBOND_REASON_AUTH_REJECTED;
        } else if (reason == AbstractionLayer.BT_STATUS_AUTH_TIMEOUT) {
            return BluetoothDevice.UNBOND_REASON_AUTH_TIMEOUT;
        }

        /* default */
        return BluetoothDevice.UNBOND_REASON_REMOVED;
    }
}
