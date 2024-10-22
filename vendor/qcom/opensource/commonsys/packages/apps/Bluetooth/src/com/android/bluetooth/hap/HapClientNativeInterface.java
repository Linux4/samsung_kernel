/*
 * Copyright 2021 HIMSA II K/S - www.himsa.com.
 * Represented by EHIMA - www.ehima.com
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

package com.android.bluetooth.hap;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHapPresetInfo;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Hearing Access Profile Client Native Interface to/from JNI.
 */
public class HapClientNativeInterface {
    private static final String TAG = "HapClientNativeInterface";
    private static final boolean DBG = true;
    private final BluetoothAdapter mAdapter;

    @GuardedBy("INSTANCE_LOCK")
    private static HapClientNativeInterface sInstance;
    private static final Object INSTANCE_LOCK = new Object();

    static {
        classInitNative();
    }

    private HapClientNativeInterface() {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mAdapter == null) {
            Log.wtf(TAG, "No Bluetooth Adapter Available");
        }
    }

    /**
     * Get singleton instance.
     */
    public static HapClientNativeInterface getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new HapClientNativeInterface();
            }
            return sInstance;
        }
    }

    /**
     * Initiates HapClientService connection to a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean connectHapClient(BluetoothDevice device) {
        return connectHapClientNative(getByteAddress(device));
    }

    /**
     * Disconnects HapClientService from a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean disconnectHapClient(BluetoothDevice device) {
        return disconnectHapClientNative(getByteAddress(device));
    }

    /**
     * Gets a HapClientService device
     *
     * @param address the remote device address
     * @return Bluetooth Device.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public BluetoothDevice getDevice(byte[] address) {
        return mAdapter.getRemoteDevice(address);
    }

    private byte[] getByteAddress(BluetoothDevice device) {
        if (device == null) {
            return Utils.getBytesFromAddress("00:00:00:00:00:00");
        }
        return Utils.getBytesFromAddress(device.getAddress());
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    void sendMessageToService(HapClientStackEvent event) {
        HapClientService service = HapClientService.getHapClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.e(TAG, "Event ignored, service not available: " + event);
        }
    }

    /**
     * Initializes the native interface.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void init() {
        initNative();
    }

    /**
     * Cleanup the native interface.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void cleanup() {
        cleanupNative();
    }

    /**
     * Selects the currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     * @param presetIndex is an index of one of the available presets
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void selectActivePreset(BluetoothDevice device, int presetIndex) {
        selectActivePresetNative(getByteAddress(device), presetIndex);
    }

    /**
     * Selects the currently active preset for a HA device group.
     *
     * @param groupId is the device group identifier for which want to set the active preset
     * @param presetIndex is an index of one of the available presets
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void groupSelectActivePreset(int groupId, int presetIndex) {
        groupSelectActivePresetNative(groupId, presetIndex);
    }

    /**
     * Sets the next preset as a currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void nextActivePreset(BluetoothDevice device) {
        nextActivePresetNative(getByteAddress(device));
    }

    /**
     * Sets the next preset as a currently active preset for a HA device group
     *
     * @param groupId is the device group identifier for which want to set the active preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void groupNextActivePreset(int groupId) {
        groupNextActivePresetNative(groupId);
    }

    /**
     * Sets the previous preset as a currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void previousActivePreset(BluetoothDevice device) {
        previousActivePresetNative(getByteAddress(device));
    }

    /**
     * Sets the previous preset as a currently active preset for a HA device group
     *
     * @param groupId is the device group identifier for which want to set the active preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void groupPreviousActivePreset(int groupId) {
        groupPreviousActivePresetNative(groupId);
    }

    /**
     * Requests the preset name
     *
     * @param device is the device for which we want to get the preset name
     * @param presetIndex is an index of one of the available presets
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void getPresetInfo(BluetoothDevice device, int presetIndex) {
        getPresetInfoNative(getByteAddress(device), presetIndex);
    }

     /**
     * Sets the preset name
     *
     * @param device is the device for which we want to get the preset name
     * @param presetIndex is an index of one of the available presets
     * @param name is a new name for a preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setPresetName(BluetoothDevice device, int presetIndex, String name) {
        setPresetNameNative(getByteAddress(device), presetIndex, name);
    }

    /**
     * Sets the preset name
     *
     * @param groupId is the device group
     * @param presetIndex is an index of one of the available presets
     * @param name is a new name for a preset
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void groupSetPresetName(int groupId, int presetIndex, String name) {
        groupSetPresetNameNative(groupId, presetIndex, name);
    }

    // Callbacks from the native stack back into the Java framework.
    // All callbacks are routed via the Service which will disambiguate which
    // state machine the message should be routed to.

    @VisibleForTesting
    void onConnectionStateChanged(int state, byte[] address) {
        HapClientStackEvent event =
                new HapClientStackEvent(
                        HapClientStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        event.device = getDevice(address);
        event.valueInt1 = state;

        if (DBG) {
            Log.d(TAG, "onConnectionStateChanged: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onDeviceAvailable(byte[] address, int features) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_DEVICE_AVAILABLE);
        event.device = getDevice(address);
        event.valueInt1 = features;

        if (DBG) {
            Log.d(TAG, "onDeviceAvailable: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onFeaturesUpdate(byte[] address, int features) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_DEVICE_FEATURES);
        event.device = getDevice(address);
        event.valueInt1 = features;

        if (DBG) {
            Log.d(TAG, "onFeaturesUpdate: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onActivePresetSelected(byte[] address, int presetIndex) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED);
        event.device = getDevice(address);
        event.valueInt1 = presetIndex;

        if (DBG) {
            Log.d(TAG, "onActivePresetSelected: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onActivePresetGroupSelected(int groupId, int presetIndex) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED);
        event.valueInt1 = presetIndex;
        event.valueInt2 = groupId;

        if (DBG) {
            Log.d(TAG, "onActivePresetGroupSelected: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onActivePresetSelectError(byte[] address, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR);
        event.device = getDevice(address);
        event.valueInt1 = resultCode;

        if (DBG) {
            Log.d(TAG, "onActivePresetSelectError: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onActivePresetGroupSelectError(int groupId, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR);
        event.valueInt1 = resultCode;
        event.valueInt2 = groupId;

        if (DBG) {
            Log.d(TAG, "onActivePresetGroupSelectError: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onPresetInfo(byte[] address, int infoReason, BluetoothHapPresetInfo[] presets) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO);
        event.device = getDevice(address);
        event.valueInt2 = infoReason;
        event.valueList = new ArrayList<>(Arrays.asList(presets));

        if (DBG) {
            Log.d(TAG, "onPresetInfo: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onGroupPresetInfo(int groupId, int infoReason, BluetoothHapPresetInfo[] presets) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO);
        event.valueInt2 = infoReason;
        event.valueInt3 = groupId;
        event.valueList = new ArrayList<>(Arrays.asList(presets));

        if (DBG) {
            Log.d(TAG, "onPresetInfo: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onPresetNameSetError(byte[] address, int presetIndex, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_NAME_SET_ERROR);
        event.device = getDevice(address);
        event.valueInt1 = resultCode;
        event.valueInt2 = presetIndex;

        if (DBG) {
            Log.d(TAG, "OnPresetNameSetError: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onGroupPresetNameSetError(int groupId, int presetIndex, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_NAME_SET_ERROR);
        event.valueInt1 = resultCode;
        event.valueInt2 = presetIndex;
        event.valueInt3 = groupId;

        if (DBG) {
            Log.d(TAG, "OnPresetNameSetError: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onPresetInfoError(byte[] address, int presetIndex, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO_ERROR);
        event.device = getDevice(address);
        event.valueInt1 = resultCode;
        event.valueInt2 = presetIndex;

        if (DBG) {
            Log.d(TAG, "onPresetInfoError: " + event);
        }
        sendMessageToService(event);
    }

    @VisibleForTesting
    void onGroupPresetInfoError(int groupId, int presetIndex, int resultCode) {
        HapClientStackEvent event = new HapClientStackEvent(
                HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO_ERROR);
        event.valueInt1 = resultCode;
        event.valueInt2 = presetIndex;
        event.valueInt3 = groupId;

        if (DBG) {
            Log.d(TAG, "onPresetInfoError: " + event);
        }
        sendMessageToService(event);
    }

    // Native methods that call into the JNI interface
    private static native void classInitNative();
    private native void initNative();
    private native void cleanupNative();
    private native boolean connectHapClientNative(byte[] address);
    private native boolean disconnectHapClientNative(byte[] address);
    private native void selectActivePresetNative(byte[] byteAddress, int presetIndex);
    private native void groupSelectActivePresetNative(int groupId, int presetIndex);
    private native void nextActivePresetNative(byte[] byteAddress);
    private native void groupNextActivePresetNative(int groupId);
    private native void previousActivePresetNative(byte[] byteAddress);
    private native void groupPreviousActivePresetNative(int groupId);
    private native void getPresetInfoNative(byte[] byteAddress, int presetIndex);
    private native void setPresetNameNative(byte[] byteAddress, int presetIndex, String name);
    private native void groupSetPresetNameNative(int groupId, int presetIndex, String name);
}
