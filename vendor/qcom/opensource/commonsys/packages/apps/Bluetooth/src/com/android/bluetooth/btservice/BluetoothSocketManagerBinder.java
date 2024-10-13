/*
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

package com.android.bluetooth.btservice;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.IBluetoothSocketManager;
import android.os.Binder;
import android.os.ParcelFileDescriptor;
import android.os.ParcelUuid;
import com.android.bluetooth.Utils;

class BluetoothSocketManagerBinder extends IBluetoothSocketManager.Stub {
    private static final String TAG = "BluetoothSocketManagerBinder";

    private static final int INVALID_FD = -1;

    private AdapterService mService;

    BluetoothSocketManagerBinder(AdapterService service) {
        mService = service;
    }

    void cleanUp() {
        mService = null;
    }

    @Override
    public ParcelFileDescriptor connectSocket(
            BluetoothDevice device, int type, ParcelUuid uuid, int port, int flag) {

        enforceActiveUser();

        if (!Utils.checkConnectPermissionForPreflight(mService)) {
            return null;
        }

        return marshalFd(mService.connectSocketNative(
            Utils.getBytesFromAddress(device.getAddress()),
            type,
            Utils.uuidToByteArray(uuid),
            port,
            flag,
            Binder.getCallingUid()));
    }

    @Override
    public ParcelFileDescriptor createSocketChannel(
            int type, String serviceName, ParcelUuid uuid, int port, int flag) {

        enforceActiveUser();

        if (!Utils.checkConnectPermissionForPreflight(mService)) {
            return null;
        }

        return marshalFd(mService.createSocketChannelNative(
            type,
            serviceName,
            Utils.uuidToByteArray(uuid),
            port,
            flag,
            Binder.getCallingUid()));

    }

    @Override
    public void requestMaximumTxDataLength(BluetoothDevice device) {
        enforceActiveUser();

        if (!Utils.checkConnectPermissionForPreflight(mService)) {
            return;
        }

        mService.requestMaximumTxDataLengthNative(Utils.getBytesFromAddress(device.getAddress()));
    }

    private void enforceActiveUser() {
        if (!Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)) {
            throw new SecurityException("Not allowed for non-active user");
        }
    }

    private static ParcelFileDescriptor marshalFd(int fd) {
        if (fd == INVALID_FD) {
            return null;
        }
        return ParcelFileDescriptor.adoptFd(fd);
    }
}
