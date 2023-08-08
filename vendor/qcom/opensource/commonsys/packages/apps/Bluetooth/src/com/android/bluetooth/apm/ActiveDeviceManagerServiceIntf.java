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

import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ActiveDeviceManagerServiceIntf {
    public static final String TAG = "APM: ActiveDeviceManagerServiceIntf";
    private static ActiveDeviceManagerServiceIntf mInterface = null;

    static Class activeDeviceManagerService = null;
    static Object mActiveDeviceManagerService = null;

    public static int SHO_SUCCESS = 0;
    public static int SHO_PENDING = 1;
    public static int SHO_FAILED = 2;
    public static int ALREADY_ACTIVE = 3;

    private ActiveDeviceManagerServiceIntf() {

    }

    public static ActiveDeviceManagerServiceIntf get() {
        if(mInterface == null) {
            mInterface = new ActiveDeviceManagerServiceIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        get();
        mActiveDeviceManagerService = obj;

        try {
            activeDeviceManagerService = Class.forName("com.android.bluetooth.apm.ActiveDeviceManagerService");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class ActiveDeviceManagerService not present. " + ex);
            activeDeviceManagerService = null;
            mActiveDeviceManagerService = null;
        }

    }

    public boolean setActiveDevice(BluetoothDevice device, int mAudioType, boolean isUIReq, boolean playReq) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[4];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Boolean.class;
        arg[3] = Boolean.class;

        Log.d(TAG, "setActiveDevice at Intf: " + device );
        try {
            Method setActiveDevice = activeDeviceManagerService.getDeclaredMethod("setActiveDevice", arg);
            Boolean ret = (Boolean) setActiveDevice.invoke(mActiveDeviceManagerService,
                        device, mAudioType, isUIReq, playReq);
            Log.d(TAG, "setActiveDevice returned at Intf: " + ret );
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

    public boolean setActiveDevice(BluetoothDevice device, int mAudioType, boolean isUIReq) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Boolean.class;

        try {
            Method setActiveDevice = activeDeviceManagerService.getDeclaredMethod("setActiveDevice", arg);
            Boolean ret = (Boolean)setActiveDevice.invoke(mActiveDeviceManagerService, device, mAudioType, isUIReq);
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

    public boolean isRecordingActive(BluetoothDevice device) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method isRecordingActive = activeDeviceManagerService.getDeclaredMethod("isRecordingActive", arg);
            Boolean ret = (Boolean)isRecordingActive.invoke(mActiveDeviceManagerService, device);
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
    public boolean suspendRecording(Boolean suspend) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = Boolean.class;

        try {
            Method suspendRecording = activeDeviceManagerService.getDeclaredMethod("suspendRecording", arg);
            Boolean ret = (Boolean)suspendRecording.invoke(mActiveDeviceManagerService, suspend);
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

    public boolean setActiveDevice(BluetoothDevice device, int mAudioType) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setActiveDevice = activeDeviceManagerService.getDeclaredMethod("setActiveDevice", arg);
            Boolean ret = (Boolean)setActiveDevice.invoke(mActiveDeviceManagerService, device, mAudioType);
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

    public boolean setActiveDeviceBlocking(BluetoothDevice device, int mAudioType) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        Log.i(TAG, "setActiveDeviceBlocking reflection- calling into adv audio to get method");
        try {
            Method setActiveDeviceBlocking = activeDeviceManagerService.getDeclaredMethod("setActiveDeviceBlocking", arg);
            Boolean ret = (Boolean)setActiveDeviceBlocking.invoke(mActiveDeviceManagerService, device, mAudioType);
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

    public boolean removeActiveDevice(int mAudioType, boolean forceStopAudio) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = Integer.class;
        arg[1] = Boolean.class;

        try {
            Method removeActiveDevice = activeDeviceManagerService.getDeclaredMethod("removeActiveDevice", arg);
            Boolean ret = (Boolean)removeActiveDevice.invoke(mActiveDeviceManagerService, mAudioType, forceStopAudio);
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

    public BluetoothDevice getActiveDevice(int mAudioType) {
        if(activeDeviceManagerService == null)
            return null;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method getActiveDevice = activeDeviceManagerService.getDeclaredMethod("getActiveDevice", arg);
            BluetoothDevice ret = (BluetoothDevice)getActiveDevice.invoke(mActiveDeviceManagerService, mAudioType);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return null;

    }

    public BluetoothDevice getActiveAbsoluteDevice(int mAudioType) {
        if(activeDeviceManagerService == null)
            return null;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method getActiveDevice = activeDeviceManagerService.getDeclaredMethod("getActiveAbsoluteDevice", arg);
            BluetoothDevice ret = (BluetoothDevice)getActiveDevice.invoke(mActiveDeviceManagerService, mAudioType);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return null;
    }

    public int getActiveProfile(int mAudioType) {
        if(activeDeviceManagerService == null)
            return -1;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method getActiveProfile = activeDeviceManagerService.getDeclaredMethod("getActiveProfile", arg);
            int ret = (int)getActiveProfile.invoke(mActiveDeviceManagerService, mAudioType);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return -1;
    }

    public boolean onActiveDeviceChange(BluetoothDevice device, int mAudioType) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method onActiveDeviceChange = activeDeviceManagerService.getDeclaredMethod("onActiveDeviceChange", arg);
            Boolean ret = (Boolean)onActiveDeviceChange.invoke(mActiveDeviceManagerService, device, mAudioType);
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

    public boolean onActiveDeviceChange(BluetoothDevice device, int mAudioType, int mProfile) {
        if(activeDeviceManagerService == null)
            return false;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onActiveDeviceChange = activeDeviceManagerService.getDeclaredMethod("onActiveDeviceChange", arg);
            Boolean ret = (Boolean)onActiveDeviceChange.invoke(mActiveDeviceManagerService, device, mAudioType, mProfile);
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
}


