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

package com.android.bluetooth.vc;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.util.Log;
import android.util.StatsLog;
import java.util.List;
import java.util.ArrayList;
import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import com.android.internal.annotations.VisibleForTesting;

public class VcpControllerIntf {
    public static final String TAG = "VCP: VcpControllerIntf";
    private static VcpControllerIntf mInterface = null;

    static Class VcpController = null;
    static Object mVcpController = null;

    private VcpControllerIntf() {

    }

    public static VcpControllerIntf get() {
        if(mInterface == null) {
            mInterface = new VcpControllerIntf();
        }
        return mInterface;
    }

    public static void init (Object obj) {
        Log.i(TAG, "init");
        mVcpController = obj;

        try {
            VcpController = Class.forName("com.android.bluetooth.vcp.VcpController");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class VcpController not present. " + ex);
            VcpController = null;
            mVcpController = null;
        }
    }

    public boolean connect(BluetoothDevice device) {
        if(VcpController == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method connect = VcpController.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mVcpController, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    }

    public boolean disconnect(BluetoothDevice device) {
        if(VcpController == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disconnect = VcpController.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mVcpController, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(VcpController == null)
            return devices;

        try {
            Method getConnectedDevices = VcpController.getDeclaredMethod("getConnectedDevices");
            List<BluetoothDevice>  ret = (List<BluetoothDevice>)getConnectedDevices.invoke(mVcpController);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return devices;
    }

    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(VcpController == null)
            return devices;

        Class[] arg = new Class[1];
        arg[0] = int[].class;

        try {
            Method getDevicesMatchingConnectionStates =
                    VcpController.getDeclaredMethod("getDevicesMatchingConnectionStates", arg);
            List<BluetoothDevice>  ret =
                    (List<BluetoothDevice>)getDevicesMatchingConnectionStates.invoke(mVcpController, states);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return devices;
    }

    public int getConnectionState(BluetoothDevice device) {
        if(VcpController == null)
            return BluetoothProfile.STATE_DISCONNECTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionState = VcpController.getDeclaredMethod("getConnectionState", arg);
            int ret = (int)getConnectionState.invoke(mVcpController, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return BluetoothProfile.STATE_DISCONNECTED;
    }

    public void setVolumeControlSevice(VolumeControlService volumeControlService) {
        if(VcpController == null) {
            return;
        }
        Class[] args = new Class[1];
        args[0] = VolumeControlService.class;
        try {
            Method setVolumeControlSevice = VcpController.getDeclaredMethod(
                    "setVolumeControlSevice", args);
            setVolumeControlSevice.invoke(mVcpController, volumeControlService);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }
}
