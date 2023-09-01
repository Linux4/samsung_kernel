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

package com.android.bluetooth.groupclient;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import java.util.ArrayList;
import java.util.List;
import android.util.Log;
import java.util.UUID;

import com.android.bluetooth.Utils;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

/**
 * CSIP Client Native Interface to/from JNI.
 */
public class GroupClientNativeInterface {
    private static final String TAG = "BluetoothGroupNativeIntf";
    private static final boolean DBG = true;
    private BluetoothAdapter mAdapter;

    @GuardedBy("INSTANCE_LOCK")
    private static GroupClientNativeInterface sInstance;
    private static final Object INSTANCE_LOCK = new Object();

    static {
        classInitNative();
    }

    private GroupClientNativeInterface() {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        if (mAdapter == null) {
            Log.wtfStack(TAG, "No Bluetooth Adapter Available");
        }
    }

    /**
     * Get singleton instance.
     */
    public static GroupClientNativeInterface getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new GroupClientNativeInterface();
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
     * Register CSIP app with the stack code.
     *
     * @param appUuidLsb lsb of app uuid.
     * @param appUuidMsb msb of app uuid.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void registerCsipApp(long appUuidLsb, long appUuidMsb) {
       registerCsipAppNative(appUuidLsb, appUuidMsb);
    }

    /**
     * Register CSIP app with the stack code.
     *
     * @param appId ID of the application to be unregistered.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void unregisterCsipApp(int appId) {
       unregisterCsipAppNative(appId);
    }

    /**
     * Change lock value of the coordinated set member
     *
     * @param appId ID of the application which is requesting change in lock status
     * @param setId Identifier of the set
     * @param devices List of bluetooth devices for whick lock status change is required
     * @param value Lock/Unlock value
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setLockValue(int appId, int setId, List<BluetoothDevice> devices,
             int value) {
       int i = 0;
       int size = ((devices != null) ? devices.size() : 0);
       String[] devicesList = new String[size];
       if (size > 0) {
           for (BluetoothDevice device: devices) {
                devicesList[i++] = device.toString();
           }
       }
       setLockValueNative(appId, setId, value, devicesList);
    }

    /**
     * Initiates Csip connection to a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean connectSetDevice(int appId, BluetoothDevice device) {
        return connectSetDeviceNative(appId, getByteAddress(device));
    }

    /**
     * Disconnects Csip from a remote device.
     *
     * @param device the remote device
     * @return true on success, otherwise false.
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean disconnectSetDevice(int appId, BluetoothDevice device) {
        return disconnectSetDeviceNative(appId, getByteAddress(device));
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

    private void onCsipAppRegistered (int status, int appId,
            long uuidLsb, long uuidMsb) {
        UUID uuid = new UUID(uuidMsb, uuidLsb);

        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onCsipAppRegistered(status, appId, uuid);
        }
    }

    private void onConnectionStateChanged(int appId, String bdAddr,
            int state, int status) {
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter()
                                                 .getRemoteDevice(bdAddr);

        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onConnectionStateChanged (appId, device, state, status);
        }
    }

    private void onNewSetFound (int setId, String bdAddr, int size, byte[] sirk,
            long uuidLsb, long uuidMsb, boolean lockSupport) {
        UUID uuid = new UUID(uuidMsb, uuidLsb);
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter()
                                                 .getRemoteDevice(bdAddr);

        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onNewSetFound(setId, device, size, sirk, uuid, lockSupport);
        }
    }

    private void onNewSetMemberFound (int setId, String bdAddr) {
        BluetoothDevice device =
                BluetoothAdapter.getDefaultAdapter().getRemoteDevice(bdAddr);

        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onNewSetMemberFound(setId, device);
        }
    }

    private void onLockStatusChanged (int appId, int setId, int value,
            int status, String[] bdAddr) {
        List<BluetoothDevice> lockMembers = new ArrayList<BluetoothDevice>();
        for (String address: bdAddr) {
            lockMembers.add(mAdapter.getRemoteDevice(address.toUpperCase()));
        }

        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onLockStatusChanged(appId, setId, value, status, lockMembers);
        }
    }

    private void onLockAvailable (int appId, int setId, String address) {
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onLockAvailable(appId, setId, device);
        }
    }

    private void onSetSizeChanged (int setId, int size, String address) {
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onSetSizeChanged(setId, size, device);
        }
    }

    private void onSetSirkChanged(int setId, byte[] sirk, String address) {
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        GroupService service = GroupService.getGroupService();
        if (service != null) {
            service.onSetSirkChanged(setId, sirk, device);
        }
    }

    // Native methods that call JNI interface
    private static native void classInitNative();
    private native void initNative();
    private native void cleanupNative();
    private native void registerCsipAppNative(long appUuidLsb, long appUuidMsb);
    private native void unregisterCsipAppNative(int appId);
    private native void setLockValueNative(int appId, int setId, int value, String[] devicesList);
    private native boolean connectSetDeviceNative(int appId, byte[] address);
    private native boolean disconnectSetDeviceNative(int appId, byte[] address);
}
