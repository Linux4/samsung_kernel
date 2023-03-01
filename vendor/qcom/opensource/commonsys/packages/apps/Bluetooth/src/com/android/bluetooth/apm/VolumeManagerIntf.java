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
import android.content.Intent;
import android.util.Log;
import android.util.StatsLog;

import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class VolumeManagerIntf {
    public static final String TAG = "APM: VolumeManagerIntf";
    private static VolumeManagerIntf mInterface = null;

    static Class VolumeManager = null;
    static Object mVolumeManager = null;

    private int safeVol = 7;

    private VolumeManagerIntf() {

    }

    public static VolumeManagerIntf get() {
        if(mInterface == null) {
            mInterface = new VolumeManagerIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mVolumeManager = obj;

        try {
            VolumeManager = Class.forName("com.android.bluetooth.apm.VolumeManager");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class VolumeManager not present. " + ex);
            VolumeManager = null;
            mVolumeManager = null;
        }
    }

    public void setMediaAbsoluteVolume (int volume) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method setMediaAbsoluteVolume = VolumeManager.getDeclaredMethod("setMediaAbsoluteVolume", arg);
            setMediaAbsoluteVolume.invoke(mVolumeManager, volume);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setLeAudioVolume (int volume) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method setLeAudioVolume = VolumeManager.getDeclaredMethod("setLeAudioVolume", arg);
            setLeAudioVolume.invoke(mVolumeManager, volume);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void updateMediaStreamVolume (int volume) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method updateMediaStreamVolume = VolumeManager.getDeclaredMethod("updateMediaStreamVolume", arg);
            updateMediaStreamVolume.invoke(mVolumeManager, volume);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setCallVolume (Intent intent) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = Intent.class;

        try {
            Method setCallVolume = VolumeManager.getDeclaredMethod("setCallVolume", arg);
            setCallVolume.invoke(mVolumeManager, intent);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onConnStateChange(BluetoothDevice device, int state, int profile) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onConnStateChange = VolumeManager.getDeclaredMethod("onConnStateChange", arg);
            onConnStateChange.invoke(mVolumeManager, device, state, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onVolumeChange(int volume, int audioType, boolean showUI) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = Integer.class;
        arg[1] = Integer.class;
        arg[2] = Boolean.class;

        try {
            Method onVolumeChange = VolumeManager.getDeclaredMethod("onVolumeChange", arg);
            onVolumeChange.invoke(mVolumeManager, volume, audioType, showUI);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onVolumeChange(BluetoothDevice device, int volume, int audioType) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onVolumeChange = VolumeManager.getDeclaredMethod("onVolumeChange", arg);
            onVolumeChange.invoke(mVolumeManager, device, volume, audioType);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setAbsoluteVolumeSupport(BluetoothDevice device, boolean isSupported, int initVol, int profile) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[4];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;
        arg[2] = Integer.class;
        arg[3] = Integer.class;

        try {
            Method setAbsoluteVolumeSupport = VolumeManager.getDeclaredMethod("setAbsoluteVolumeSupport", arg);
            setAbsoluteVolumeSupport.invoke(mVolumeManager, device, isSupported, initVol, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setAbsoluteVolumeSupport(BluetoothDevice device, boolean isSupported, int profile) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;
        arg[2] = Integer.class;

        try {
            Method setAbsoluteVolumeSupport = VolumeManager.getDeclaredMethod("setAbsoluteVolumeSupport", arg);
            setAbsoluteVolumeSupport.invoke(mVolumeManager, device, isSupported, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void saveVolume(int audioType) {
        if(VolumeManager == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method saveVolume = VolumeManager.getDeclaredMethod("saveVolume", arg);
            saveVolume.invoke(mVolumeManager, audioType);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int getSavedVolume(BluetoothDevice device, int audioType) {
        if(VolumeManager == null)
            return safeVol;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method getSavedVolume = VolumeManager.getDeclaredMethod("getSavedVolume", arg);
            Integer ret = (Integer)getSavedVolume.invoke(mVolumeManager, device, audioType);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return safeVol;
    }

    public int getActiveVolume(int audioType) {
        if(VolumeManager == null)
            return safeVol;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method getActiveVolume = VolumeManager.getDeclaredMethod("getActiveVolume", arg);
            Integer ret = (Integer)getActiveVolume.invoke(mVolumeManager, audioType);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return safeVol;
    }
}
