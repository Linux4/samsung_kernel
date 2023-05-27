/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
         copyright notice, this list of conditions and the following
         disclaimer in the documentation and/or other materials provided
         with the distribution.
       * Neither the name of The Linux Foundation nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

package com.android.bluetooth.apm;

import android.bluetooth.BluetoothDevice;
import android.util.Log;
import android.util.StatsLog;
import java.util.List;
import java.util.ArrayList;
import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.Context;

public class DeviceProfileMapIntf {
    public static final String TAG = "APM: DeviceProfileMapIntf";
    private static DeviceProfileMapIntf mInterface = null;

    static Class DeviceProfileMap = null;
    static Object mDeviceProfileMap = null;

    private DeviceProfileMapIntf() {

    }

    public static DeviceProfileMapIntf getDeviceProfileMapInstance() {
        if(mInterface == null) {
            mInterface = new DeviceProfileMapIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mDeviceProfileMap = obj;

        try {
            DeviceProfileMap = Class.forName("com.android.bluetooth.apm.DeviceProfileMap");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class DeviceProfileMap not present. " + ex);
            DeviceProfileMap = null;
            mDeviceProfileMap = null;
        }
    }

    public int getAllSupportedProfile(BluetoothDevice device) {
        if(DeviceProfileMap == null)
            return ApmConstIntf.AudioProfiles.NONE;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getAllSupportedProfile = DeviceProfileMap.getDeclaredMethod("getAllSupportedProfile", arg);
            int ret = (int)getAllSupportedProfile.invoke(mDeviceProfileMap, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return ApmConstIntf.AudioProfiles.NONE;
    }

    public int getProfile(BluetoothDevice device, int mAudioFeature) {
        if(DeviceProfileMap == null)
            return ApmConstIntf.AudioProfiles.NONE;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method getProfile = DeviceProfileMap.getDeclaredMethod("getProfile", arg);
            int ret = (int)getProfile.invoke(mDeviceProfileMap, device, mAudioFeature);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return ApmConstIntf.AudioProfiles.NONE;
    }

    public void profileDescoveryUpdate (BluetoothDevice device, int mAudioProfile) {
        if(DeviceProfileMap == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method profileDescoveryUpdate = DeviceProfileMap.getDeclaredMethod("profileDescoveryUpdate", arg);
            profileDescoveryUpdate.invoke(mDeviceProfileMap, device, mAudioProfile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void profileConnectionUpdate (BluetoothDevice device, int mAudioFeature,
                int mAudioProfile, boolean mProfileStatus) {
        if(DeviceProfileMap == null)
            return;

        Class[] arg = new Class[4];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;
        arg[3] = Boolean.class;

        try {
            Method profileConnectionUpdate = DeviceProfileMap.getDeclaredMethod("profileConnectionUpdate", arg);
            profileConnectionUpdate.invoke(mDeviceProfileMap, device, mAudioFeature, mAudioProfile, mProfileStatus);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setActiveProfile(BluetoothDevice device, int mAudioFeature, int mAudioProfile) {
        if(DeviceProfileMap == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method setActiveProfile = DeviceProfileMap.getDeclaredMethod("setActiveProfile", arg);
            setActiveProfile.invoke(mDeviceProfileMap, device, mAudioFeature, mAudioProfile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }
}
