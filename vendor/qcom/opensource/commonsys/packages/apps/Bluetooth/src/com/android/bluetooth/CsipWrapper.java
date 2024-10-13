/*
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth;

import java.util.List;

import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.Config;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.bluetooth.csip.CsipSetCoordinatorService;
import com.android.bluetooth.groupclient.GroupService;

import android.bluetooth.BluetoothGroupCallback;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.DeviceGroup;
import android.content.Context;
import android.os.ParcelUuid;
import android.util.Log;

/**
 * Wrapper class for all internal CSIP apis
 */
public class CsipWrapper {

    private static CsipWrapper sInstance;

    /** Used when obtaining a reference to the singleton instance. */
    private static final Object INSTANCE_LOCK = new Object();
    private GroupService mGroupService;
    private CsipSetCoordinatorService mCsipSetCoordinatorService;
    private final String TAG = "CsipWrapper";
    private final boolean DBG = true;

    /**
     * Get singleton instance.
    */
    public static CsipWrapper getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new CsipWrapper();
            }
            return sInstance;
        }
    }

    public void connect(int appId, BluetoothDevice device) {
        if (getGroupService() != null) {
            mGroupService.connect(appId, device);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.connect(appId, device);
        } else {
            if (DBG)
                Log.d(TAG, "connect not called");
        }
    }

    public void disconnect(int appId, BluetoothDevice device) {
        if (getGroupService() != null) {
            mGroupService.disconnect(appId, device);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.disconnect(appId, device);
        } else {
            if (DBG)
                Log.d(TAG, "disconnect not called");
        }
    }

    public void setLockValue(int appId, int setId, List<BluetoothDevice> devices, int lockVal) {
        if (getGroupService() != null) {
            mGroupService.setLockValue(appId, setId, devices, lockVal);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.setLockValue(appId, setId, devices, lockVal);
        } else {
            if (DBG)
                Log.d(TAG, "setLockValue not called");
        }
    }

    public int getRemoteDeviceGroupId(BluetoothDevice device, ParcelUuid uuid) {
        int setid = -1;
        if (getGroupService() != null) {
            setid = mGroupService.getRemoteDeviceGroupId(device, uuid);
        } else if (getCsipSetCoordinatorService() != null) {
            setid = mCsipSetCoordinatorService.getRemoteDeviceGroupId(device, uuid);
        } else {
            if (DBG)
                Log.d(TAG, "getRemoteDeviceGroupId not called");
        }
        return setid;
    }

    public void registerGroupClientModule(BluetoothGroupCallback callbacks) {
        if (getGroupService() != null) {
            mGroupService.registerGroupClientModule(callbacks);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.registerGroupClientModule(callbacks);
        } else {
            if (DBG)
                Log.d(TAG, "registerGroupClientModule not called");
        }
    }

    public void unregisterGroupClientModule(int appid) {
        if (getGroupService() != null) {
            mGroupService.unregisterGroupClientModule(appid);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.unregisterGroupClientModule(appid);
        } else {
            if (DBG)
                Log.d(TAG, "unregisterGroupClientModule not called");
        }
    }

    public DeviceGroup getCoordinatedSet(int setId) {
        DeviceGroup deviceGroup = null;
        if (getGroupService() != null) {
            deviceGroup = mGroupService.getCoordinatedSet(setId);
        } else if (getCsipSetCoordinatorService() != null) {
            deviceGroup = mCsipSetCoordinatorService.getCoordinatedSet(setId);
        } else {
            if (DBG)
                Log.d(TAG, "getCoordinatedSet not called");
        }
        return deviceGroup;
    }

    public void loadDeviceGroupFromBondedDevice(BluetoothDevice device, String setDetails) {
        if (Config.getIsCsipQti()) {
            GroupService.loadDeviceGroupFromBondedDevice(device, setDetails);
        } else {
            CsipSetCoordinatorService.loadDeviceGroupFromBondedDevice(device, setDetails);
        }
    }

    public void handleEIRGroupData(BluetoothDevice device, String data) {
        if (getGroupService() != null) {
            mGroupService.handleEIRGroupData(device, data);
        } else if (getCsipSetCoordinatorService() != null) {
            mCsipSetCoordinatorService.handleEIRGroupData(device, data);
        } else {
            if (DBG)
                Log.d(TAG, "handleEIRGroupData not called");
        }
    }

    private GroupService getGroupService() {
        if (mGroupService == null) {
            mGroupService = new ServiceFactory().getGroupService();
        }
        return mGroupService;
    }

    private CsipSetCoordinatorService getCsipSetCoordinatorService() {
        if (mCsipSetCoordinatorService == null) {
            mCsipSetCoordinatorService =
                new ServiceFactory().getCsipSetCoordinatorService();
        }
        return mCsipSetCoordinatorService;
    }

    public boolean isCsipEnabled() {
        return (getGroupService() != null) || (getCsipSetCoordinatorService() != null);
    }
}
