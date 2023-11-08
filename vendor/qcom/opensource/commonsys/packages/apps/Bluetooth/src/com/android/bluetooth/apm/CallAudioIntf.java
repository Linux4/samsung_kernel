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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
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
import android.bluetooth.BluetoothStatusCodes;

public class CallAudioIntf {
    public static final String TAG = "APM: CallAudioIntf";
    private static CallAudioIntf mInterface = null;

    static Class CallAudio = null;
    static Object mCallAudio = null;

    private CallAudioIntf() {

    }

    public static CallAudioIntf get() {
        if(mInterface == null) {
            mInterface = new CallAudioIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mCallAudio = obj;

        try {
            CallAudio = Class.forName("com.android.bluetooth.apm.CallAudio");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class CallAudio not present. " + ex);
            CallAudio = null;
            mCallAudio = null;
        }
    }

    public boolean connect(BluetoothDevice device) {
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method connect = CallAudio.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mCallAudio, device);
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

    public boolean connect(BluetoothDevice device, boolean allProfiles) {
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method connect = CallAudio.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mCallAudio, device, allProfiles);
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

    public boolean autoConnect(BluetoothDevice device) {
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method autoConnect = CallAudio.getDeclaredMethod("autoConnect", arg);
            Boolean ret = (Boolean)autoConnect.invoke(mCallAudio, device);
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
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disconnect = CallAudio.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mCallAudio, device);
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

    public boolean disconnect(BluetoothDevice device, boolean allProfiles) {
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method disconnect = CallAudio.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mCallAudio, device, allProfiles);
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

    public boolean startScoUsingVirtualVoiceCall() {
        if(CallAudio == null)
            return false;

        try {
            Method startScoUsingVirtualVoiceCall = CallAudio.getDeclaredMethod("startScoUsingVirtualVoiceCall");
            Boolean ret = (Boolean)startScoUsingVirtualVoiceCall.invoke(mCallAudio);
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

    public boolean stopScoUsingVirtualVoiceCall() {
        if(CallAudio == null)
            return false;

        try {
            Method stopScoUsingVirtualVoiceCall = CallAudio.getDeclaredMethod("stopScoUsingVirtualVoiceCall");
            Boolean ret = (Boolean)stopScoUsingVirtualVoiceCall.invoke(mCallAudio);
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

    public void remoteDisconnectVirtualVoiceCall(BluetoothDevice device) {
        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method remoteDisconnectVirtualVoiceCall = CallAudio.getDeclaredMethod("remoteDisconnectVirtualVoiceCall", arg);
            remoteDisconnectVirtualVoiceCall.invoke(mCallAudio, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int connectAudio() {
        if(CallAudio == null)
            return BluetoothStatusCodes.ERROR_PROFILE_NOT_CONNECTED;

        try {
            Method connectAudio = CallAudio.getDeclaredMethod("connectAudio");
            int ret = (int)connectAudio.invoke(mCallAudio);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return BluetoothStatusCodes.ERROR_UNKNOWN;
    }

    public int disconnectAudio() {
        if(CallAudio == null)
            return BluetoothStatusCodes.ERROR_PROFILE_NOT_CONNECTED;;

        try {
            Method disconnectAudio = CallAudio.getDeclaredMethod("disconnectAudio");
            int ret = (int)disconnectAudio.invoke(mCallAudio);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return BluetoothStatusCodes.ERROR_UNKNOWN;
    }

    public boolean isVoiceOrCallActive() {
        if(CallAudio == null)
            return false;

        try {
            Method isVoiceOrCallActive = CallAudio.getDeclaredMethod("isVoiceOrCallActive");
            Boolean ret = (Boolean)isVoiceOrCallActive.invoke(mCallAudio);
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

    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        if(CallAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setConnectionPolicy = CallAudio.getDeclaredMethod("setConnectionPolicy", arg);
            Boolean ret = (Boolean)setConnectionPolicy.invoke(mCallAudio, device, connectionPolicy);
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

    public int getConnectionPolicy(BluetoothDevice device) {
        if(CallAudio == null)
            return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionPolicy = CallAudio.getDeclaredMethod("getConnectionPolicy", arg);
            int ret = (int)getConnectionPolicy.invoke(mCallAudio, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
    }

    public int getAudioState(BluetoothDevice device) {
        if(CallAudio == null)
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getAudioState = CallAudio.getDeclaredMethod("getAudioState", arg);
            int ret = (int)getAudioState.invoke(mCallAudio, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
    }

    public boolean isAudioOn() {
        if(CallAudio == null)
            return false;

        try {
            Method isAudioOn = CallAudio.getDeclaredMethod("isAudioOn");
            Boolean ret = (Boolean)isAudioOn.invoke(mCallAudio);
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

    public boolean isVoipLeaWarEnabled() {
        if(CallAudio == null)
            return false;

        try {
            Method isVoipLeaWarEnabled = CallAudio.getDeclaredMethod("isVoipLeaWarEnabled");
            Boolean ret = (Boolean)isVoipLeaWarEnabled.invoke(mCallAudio);
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
        if(CallAudio == null)
            return devices;

        try {
            Method getConnectedDevices = CallAudio.getDeclaredMethod("getConnectedDevices");
            List<BluetoothDevice>  ret = (List<BluetoothDevice>)getConnectedDevices.invoke(mCallAudio);
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
        if(CallAudio == null)
            return devices;

        Class[] arg = new Class[1];
        arg[0] = int[].class;

        try {
            Method getDevicesMatchingConnectionStates =
                    CallAudio.getDeclaredMethod("getDevicesMatchingConnectionStates", arg);
            List<BluetoothDevice>  ret =
                    (List<BluetoothDevice>)getDevicesMatchingConnectionStates.invoke(mCallAudio, states);
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
        if(CallAudio == null)
            return BluetoothProfile.STATE_DISCONNECTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionState = CallAudio.getDeclaredMethod("getConnectionState", arg);
            int ret = (int)getConnectionState.invoke(mCallAudio, device);
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

    public void onConnStateChange(BluetoothDevice device, int state, int profile) {
        if(CallAudio == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onConnStateChange = CallAudio.getDeclaredMethod("onConnStateChange", arg);
            onConnStateChange.invoke(mCallAudio, device, state, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
      }
    }

    public void onAudioStateChange(BluetoothDevice device, int state) {
        if(CallAudio == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method onAudioStateChange = CallAudio.getDeclaredMethod("onAudioStateChange", arg);
            onAudioStateChange.invoke(mCallAudio, device, state);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
      }
    }
}

