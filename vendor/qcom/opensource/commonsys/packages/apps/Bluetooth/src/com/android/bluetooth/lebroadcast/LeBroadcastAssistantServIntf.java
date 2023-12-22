/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

package com.android.bluetooth.lebroadcast;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothLeBroadcastMetadata;
import android.bluetooth.BluetoothLeBroadcastReceiveState;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.ScanFilter;
import android.util.Log;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/* LE Broadcast Assistant Service Reflect Interface */
public class LeBroadcastAssistantServIntf {
    public static final String TAG = "BroadcastService: LeBroadcastAssistantServIntf";

    private static LeBroadcastAssistantServIntf mInterface = null;
    static Class BCService = null;
    static Object mBCService = null;

    private LeBroadcastAssistantServIntf() {
    }

    public static LeBroadcastAssistantServIntf get() {
        if(mInterface == null) {
            mInterface = new LeBroadcastAssistantServIntf();
        }
        return mInterface;
    }

    public static void init (Object obj) {
        Log.i(TAG, "init");
        mBCService = obj;

        try {
            BCService = Class.forName(
                    "com.android.bluetooth.bc.BCService");
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Class BCService not present. " + e);
            BCService = null;
            mBCService = null;
        }
    }

    public boolean connect(BluetoothDevice device) {
        Log.i(TAG, "connect: device " + device);
        if(BCService == null) {
            return false;
        }
        Class[] args = new Class[1];
        args[0] = BluetoothDevice.class;
        try {
            Method connect = BCService.getDeclaredMethod("connect", args);
            return (boolean) connect.invoke(mBCService, device);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return false;
    }

    public boolean disconnect(BluetoothDevice device) {
        Log.i(TAG, "disconnect: device " + device);
        if(BCService == null) {
            return false;
        }
        Class[] args = new Class[1];
        args[0] = BluetoothDevice.class;
        try {
            Method disconnect = BCService.getDeclaredMethod("disconnect", args);
            return (boolean) disconnect.invoke(mBCService, device);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return false;
    }

    public int getConnectionState(BluetoothDevice sink) {
        Log.i(TAG, "getConnectionState: device " + sink);
        if(BCService == null) {
            return BluetoothProfile.STATE_DISCONNECTED;
        }
        Class[] args = new Class[1];
        args[0] = BluetoothDevice.class;
        try {
            Method getConnectionState = BCService.getDeclaredMethod("getConnectionState", args);
            return (int) getConnectionState.invoke(mBCService, sink);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return BluetoothProfile.STATE_DISCONNECTED;
    }

    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        Log.i(TAG, "getDevicesMatchingConnectionStates");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(BCService == null) {
            return devices;
        }
        Class[] args = new Class[1];
        args[0] = int[].class;
        try {
            Method getDevicesMatchingConnectionStates = BCService.getDeclaredMethod(
                    "getDevicesMatchingConnectionStates", args);
            devices = (ArrayList<BluetoothDevice>)
                    getDevicesMatchingConnectionStates.invoke(mBCService, states);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return devices;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        Log.i(TAG, "getConnectedDevices");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(BCService == null) {
            return devices;
        }
        try {
            Method getConnectedDevices = BCService.getDeclaredMethod(
                    "getConnectedDevices");
            devices = (ArrayList<BluetoothDevice>)
                    getConnectedDevices.invoke(mBCService);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return devices;
    }

    public void setBassClientSevice(BassClientService bassService) {
        if(BCService == null) {
            return;
        }
        Class[] args = new Class[1];
        args[0] = BassClientService.class;
        try {
            Method setBassClientSevice = BCService.getDeclaredMethod(
                    "setBassClientSevice", args);
            setBassClientSevice.invoke(mBCService, bassService);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void startSearchingForSources(List<ScanFilter> filters) {
        Log.i(TAG, "startSearchingForSources");
        if(BCService == null) {
            return;
        }
        Class[] args = new Class[1];
        args[0] = List.class;
        try {
            Method startSearchingForSources =
                    BCService.getDeclaredMethod("startSearchingForSources", args);
            startSearchingForSources.invoke(mBCService, filters);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void stopSearchingForSources() {
        Log.i(TAG, "stopSearchingForSources");
        if(BCService == null) {
            return;
        }
        try {
            Method stopSearchingForSources =
                    BCService.getDeclaredMethod("stopSearchingForSources");
            stopSearchingForSources.invoke(mBCService);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public boolean isSearchInProgress() {
        Log.i(TAG, "isSearchInProgress");
        if(BCService == null) {
            return false;
        }
        try {
            Method isSearchInProgress =
                    BCService.getDeclaredMethod("isSearchInProgress");
            return (boolean) isSearchInProgress.invoke(mBCService);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return false;
    }

    public void addSource(BluetoothDevice sink, BluetoothLeBroadcastMetadata sourceMetadata,
                          boolean isGroupOp) {
        Log.i(TAG, "addSource: device " + sink);
        if(BCService == null) {
            return;
        }
        Class[] args = new Class[3];
        args[0] = BluetoothDevice.class;
        args[1] = BluetoothLeBroadcastMetadata.class;
        args[2] = boolean.class;
        try {
            Method addSource =
                    BCService.getDeclaredMethod("addSource", args);
            addSource.invoke(mBCService, sink, sourceMetadata, isGroupOp);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void modifySource(BluetoothDevice sink, int sourceId,
                             BluetoothLeBroadcastMetadata updatedMetadata) {
        Log.i(TAG, "modifySource: device " + sink + " sourceId " +
                sourceId);
        if(BCService == null) {
            return;
        }
        Class[] args = new Class[3];
        args[0] = BluetoothDevice.class;
        args[1] = int.class;
        args[2] = BluetoothLeBroadcastMetadata.class;
        try {
            Method modifySource =
                    BCService.getDeclaredMethod("modifySource", args);
            modifySource.invoke(mBCService, sink, sourceId, updatedMetadata);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void removeSource(BluetoothDevice sink, int sourceId) {
        Log.i(TAG, "removeSource: device " + sink + " sourceId " +
                sourceId);
        if(BCService == null) {
            return;
        }
        Class[] args = new Class[2];
        args[0] = BluetoothDevice.class;
        args[1] = int.class;
        try {
            Method removeSource =
                    BCService.getDeclaredMethod("removeSource", args);
            removeSource.invoke(mBCService, sink, sourceId);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public List<BluetoothLeBroadcastReceiveState> getAllSources(BluetoothDevice sink) {
        Log.i(TAG, "getAllSources: device " + sink);
        ArrayList<BluetoothLeBroadcastReceiveState> recvStates = new ArrayList<>();
        if(BCService == null) {
            return recvStates;
        }
        Class[] args = new Class[1];
        args[0] = BluetoothDevice.class;
        try {
            Method getAllSources = BCService.getDeclaredMethod(
                    "getAllSources", args);
            recvStates = (ArrayList<BluetoothLeBroadcastReceiveState>)
                    getAllSources.invoke(mBCService, sink);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return recvStates;
    }

    public int getMaximumSourceCapacity(BluetoothDevice sink) {
        Log.i(TAG, "getMaximumSourceCapacity: device " + sink);
        if(BCService == null) {
            return 0;
        }
        Class[] args = new Class[1];
        args[0] = BluetoothDevice.class;
        try {
            Method getMaximumSourceCapacity =
                    BCService.getDeclaredMethod("getMaximumSourceCapacity", args);
            return (int) getMaximumSourceCapacity.invoke(mBCService, sink);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return 0;
    }
}
