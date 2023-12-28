/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

/*
 * Defines the native interface that is used by state machine/service to
 * send or receive messages from the native stack. This file is registered
 * for the native methods in the corresponding JNI C++ file.
 */
package com.android.bluetooth.directionfinder;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

/**
 * Atp Locator Native Interface to/from JNI.
 */
public class AtpLocatorNativeInterface {
    private static final String TAG = "AtpLocatorNativeInterface";
    private static final boolean DBG = true;
    private BluetoothAdapter mAdapter;

    @GuardedBy("INSTANCE_LOCK")
    private static AtpLocatorNativeInterface sInstance;
    private static final Object INSTANCE_LOCK = new Object();

    static {
        classInitNative();
    }

    private AtpLocatorNativeInterface() {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mAdapter == null) {
            Log.wtfStack(TAG, "No Bluetooth Adapter Available");
        }
    }

    /**
     * Get singleton instance.
     */
    public static AtpLocatorNativeInterface getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new AtpLocatorNativeInterface();
            }
            return sInstance;
        }
    }

    /**
     * Initializes the native interface.
     *
     * priorities to configure.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void init() {
        Log.d(TAG, " AtpLocatorNativeInterface: init");
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
     * Initiates ATP connection to a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean connectAtp(BluetoothDevice device, boolean isDirect) {
        return connectAtpNative(getByteAddress(device), isDirect);
    }

    /**
     * Disconnects ATP from a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean disconnectAtp(BluetoothDevice device) {
        return disconnectAtpNative(getByteAddress(device));
    }

    /**
     * Enable/Disable Ble Direction Finding to remote device
     *
     * @param device: remote device instance
     * @param samplingEnable: sampling Enable
     * @param slotDurations: slot Durations
     * @param enable: CTE enable
     * @param cteReqInt: CTE request interval
     * @param reqCteLen: Requested CTE length
     * @param reqCteType: Requested CTE Type
     * @return true if Enable/Disable of Ble Direction Finding
     * succeeded.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean enableBleDirectionFinding(int samplingEnable, int slotDurations,
            int enable, int cteReqInt, int reqCteLen, int dirFindingType,
            BluetoothDevice device) {
        return enableBleDirFindingNative(samplingEnable, slotDurations,
                enable, cteReqInt, reqCteLen, dirFindingType, getByteAddress(device));
    }

    private BluetoothDevice getDevice(byte[] address) {
        return mAdapter.getRemoteDevice(address);
    }

    private byte[] getByteAddress(BluetoothDevice device) {
        if (device == null) {
            return Utils.getBytesFromAddress("00:00:00:00:00:00");
        }
        return Utils.getBytesFromAddress(device.getAddress());
    }

    private void sendMessageToService(AtpStackEvent event) {
        AtpLocator service = AtpLocator.getAtpLocator();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.e(TAG, "Event ignored, service not available: " + event);
        }
    }

    // Callbacks from the native stack back into the Java framework.
    // All callbacks are routed via the Service which will disambiguate which
    // state machine the message should be routed to.
    private void onConnectionStateChanged(int state, byte[] address) {
        AtpStackEvent event =
                new AtpStackEvent(AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        event.device = getDevice(address);
        event.valueInt1 = state;

        if (DBG) {
            Log.d(TAG, "onConnectionStateChanged: " + event);
        }
        sendMessageToService(event);
    }

    private void OnEnableBleDirectionFinding(int status, byte[] address) {
        AtpStackEvent event = new AtpStackEvent(
                AtpStackEvent.EVENT_TYPE_ENABLE_BLE_DIRECTION_FINDING);
        event.device = getDevice(address);
        event.valueInt1 = status;

        if (DBG) {
            Log.d(TAG, "OnEnableBleDirectionFinding: " + event);
        }
        sendMessageToService(event);
    }

    private void OnLeAoaResults(int status, double azimuth, int azimuthUnc,
            double elevation, int elevationUnc, byte[] address) {
        AtpStackEvent event = new AtpStackEvent(
                AtpStackEvent.EVENT_TYPE_LE_AOA_RESULTS);
        event.device = getDevice(address);
        event.valueInt1 = status;
        event.valueDouble1 = azimuth;
        event.valueInt2 = azimuthUnc;
        event.valueDouble2 = elevation;
        event.valueInt3 = elevationUnc;

        if (DBG) {
            Log.d(TAG, "OnLeAoaResults: " + event);
        }
        sendMessageToService(event);
    }

    // Native methods that call into the JNI interface
    private static native void classInitNative();
    private native void initNative();
    private native void cleanupNative();
    private native boolean connectAtpNative(byte[] address, boolean isDirect);
    private native boolean disconnectAtpNative(byte[] address);
    private native boolean enableBleDirFindingNative(int samplingEnable,
            int slotDurations, int enable, int cteReqInt, int reqCteLen,
            int dirFindingType, byte[] address);
}
