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

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothLeAudioCodecStatus;
import android.bluetooth.BluetoothLeAudioCodecConfig;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
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

public class MediaAudioIntf {
    public static final String TAG = "APM: MediaAudioIntf";
    private static MediaAudioIntf mInterface = null;

    static Class MediaAudio = null;
    static Object mMediaAudio = null;

    private MediaAudioIntf() {

    }

    public static MediaAudioIntf get() {
        if(mInterface == null) {
            mInterface = new MediaAudioIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mMediaAudio = obj;

        try {
            MediaAudio = Class.forName("com.android.bluetooth.apm.MediaAudio");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class MediaAudio not present. " + ex);
            MediaAudio = null;
            mMediaAudio = null;
        }
    }

    public boolean connect(BluetoothDevice device) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method connect = MediaAudio.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mMediaAudio, device);
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

    public boolean connect(BluetoothDevice device, boolean allProfile) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method connect = MediaAudio.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mMediaAudio, device, allProfile);
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
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method autoConnect = MediaAudio.getDeclaredMethod("autoConnect", arg);
            Boolean ret = (Boolean)autoConnect.invoke(mMediaAudio, device);
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
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disconnect = MediaAudio.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mMediaAudio, device);
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

    public boolean disconnect(BluetoothDevice device, boolean allProfile) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method disconnect = MediaAudio.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mMediaAudio, device, allProfile);
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
        if(MediaAudio == null)
            return devices;

        try {
            Method getConnectedDevices = MediaAudio.getDeclaredMethod("getConnectedDevices");
            List<BluetoothDevice>  ret = (List<BluetoothDevice>)getConnectedDevices.invoke(mMediaAudio);
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
        if(MediaAudio == null)
            return devices;

        Class[] arg = new Class[1];
        arg[0] = int[].class;

        try {
            Method getDevicesMatchingConnectionStates =
                    MediaAudio.getDeclaredMethod("getDevicesMatchingConnectionStates", arg);
            List<BluetoothDevice>  ret =
                    (List<BluetoothDevice>)getDevicesMatchingConnectionStates.invoke(mMediaAudio, states);
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
        if(MediaAudio == null)
            return BluetoothProfile.STATE_DISCONNECTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionState = MediaAudio.getDeclaredMethod("getConnectionState", arg);
            int ret = (int)getConnectionState.invoke(mMediaAudio, device);
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

    public int getPriority(BluetoothDevice device) {
        if(MediaAudio == null)
            return BluetoothProfile.PRIORITY_UNDEFINED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getPriority = MediaAudio.getDeclaredMethod("getPriority", arg);
            int ret = (int)getPriority.invoke(mMediaAudio, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return BluetoothProfile.PRIORITY_UNDEFINED;
    }

    public boolean isA2dpPlaying(BluetoothDevice device) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method isA2dpPlaying = MediaAudio.getDeclaredMethod("isA2dpPlaying", arg);
            boolean ret = (boolean)isA2dpPlaying.invoke(mMediaAudio, device);
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

    public BluetoothCodecStatus getCodecStatus(BluetoothDevice device) {
        if(MediaAudio == null)
            return null;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getCodecStatus = MediaAudio.getDeclaredMethod("getCodecStatus", arg);
            BluetoothCodecStatus ret = (BluetoothCodecStatus)getCodecStatus.invoke(mMediaAudio, device);
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

    public BluetoothLeAudioCodecStatus getLeAudioCodecStatus(int groupId) {
        if(MediaAudio == null)
            return null;

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method getLeAudioCodecStatus = MediaAudio.getDeclaredMethod("getLeAudioCodecStatus", arg);
            BluetoothLeAudioCodecStatus ret =
                    (BluetoothLeAudioCodecStatus)getLeAudioCodecStatus.invoke(mMediaAudio, groupId);
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

    public void setCodecConfigPreference(BluetoothDevice device,
                                             BluetoothCodecConfig codecConfig) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecConfig.class;

        try {
            Method setCodecConfigPreference = MediaAudio.getDeclaredMethod("setCodecConfigPreference", arg);
            setCodecConfigPreference.invoke(mMediaAudio, device, codecConfig);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setLeAudioCodecConfigPreference(int groupId,
                             BluetoothLeAudioCodecConfig inputCodecConfig,
                             BluetoothLeAudioCodecConfig outputCodecConfig) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = Integer.class;
        arg[1] = BluetoothLeAudioCodecConfig.class;
        arg[2] = BluetoothLeAudioCodecConfig.class;

        try {
            Method setLeAudioCodecConfigPreference =
                  MediaAudio.getDeclaredMethod("setLeAudioCodecConfigPreference", arg);
            setLeAudioCodecConfigPreference.invoke(mMediaAudio, groupId,
                                                   inputCodecConfig,
                                                   outputCodecConfig);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void enableOptionalCodecs(BluetoothDevice device) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method enableOptionalCodecs = MediaAudio.getDeclaredMethod("enableOptionalCodecs", arg);
            enableOptionalCodecs.invoke(mMediaAudio, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void getSupportsOptionalCodecs(BluetoothDevice device) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getSupportsOptionalCodecs = MediaAudio.getDeclaredMethod("getSupportsOptionalCodecs", arg);
            getSupportsOptionalCodecs.invoke(mMediaAudio, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int supportsOptionalCodecs(BluetoothDevice device) {
        if(MediaAudio == null)
            return BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method supportsOptionalCodecs = MediaAudio.getDeclaredMethod("supportsOptionalCodecs", arg);
            Integer ret = (Integer)supportsOptionalCodecs.invoke(mMediaAudio, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED;
    }

    public int getOptionalCodecsEnabled(BluetoothDevice device) {
        if(MediaAudio == null)
            return BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getOptionalCodecsEnabled = MediaAudio.getDeclaredMethod("getOptionalCodecsEnabled", arg);
            Integer ret = (Integer)getOptionalCodecsEnabled.invoke(mMediaAudio, device);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN;
    }

    public void disableOptionalCodecs(BluetoothDevice device) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disableOptionalCodecs = MediaAudio.getDeclaredMethod("disableOptionalCodecs", arg);
            disableOptionalCodecs.invoke(mMediaAudio, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setOptionalCodecsEnabled(BluetoothDevice device, int value) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setOptionalCodecsEnabled = MediaAudio.getDeclaredMethod("setOptionalCodecsEnabled", arg);
            setOptionalCodecsEnabled.invoke(mMediaAudio, device, value);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int getConnectionPolicy(BluetoothDevice device) {
        if(MediaAudio == null)
            return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionPolicy = MediaAudio.getDeclaredMethod("getConnectionPolicy", arg);
            int ret = (int)getConnectionPolicy.invoke(mMediaAudio, device);
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

    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setConnectionPolicy = MediaAudio.getDeclaredMethod("setConnectionPolicy", arg);
            boolean ret = (boolean)setConnectionPolicy.invoke(mMediaAudio, device, connectionPolicy);
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

    public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        if(MediaAudio == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method setSilenceMode = MediaAudio.getDeclaredMethod("setSilenceMode", arg);
            boolean ret = (boolean)setSilenceMode.invoke(mMediaAudio, device, silence);
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

    public void onConnStateChange(BluetoothDevice device, int state, int profile) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onConnStateChange = MediaAudio.getDeclaredMethod("onConnStateChange", arg);
            onConnStateChange.invoke(mMediaAudio, device, state, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onStreamStateChange(BluetoothDevice device, int streamStatus) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method onStreamStateChange = MediaAudio.getDeclaredMethod("onStreamStateChange", arg);
            onStreamStateChange.invoke(mMediaAudio, device, streamStatus);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onCodecConfigChange(BluetoothDevice device, BluetoothCodecStatus mCodecStatus, int profile) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecStatus.class;
        arg[2] = Integer.class;

        try {
            Method onCodecConfigChange = MediaAudio.getDeclaredMethod("onCodecConfigChange", arg);
            onCodecConfigChange.invoke(mMediaAudio, device, mCodecStatus, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onCodecConfigChange(BluetoothDevice device,
            BluetoothCodecStatus mCodecStatus, int profile, boolean updateAudio) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[4];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecStatus.class;
        arg[2] = Integer.class;
        arg[3] = Boolean.class;

        try {
            Method onCodecConfigChange = MediaAudio.getDeclaredMethod("onCodecConfigChange", arg);
            onCodecConfigChange.invoke(mMediaAudio, device, mCodecStatus, profile, updateAudio);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onLeCodecConfigChange(BluetoothDevice device,
            BluetoothLeAudioCodecStatus codecStatus, int profile) {
        if(MediaAudio == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothLeAudioCodecStatus.class;
        arg[2] = Integer.class;

        try {
            Method onLeCodecConfigChange = MediaAudio.getDeclaredMethod("onLeCodecConfigChange", arg);
            onLeCodecConfigChange.invoke(mMediaAudio, device, codecStatus, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }
}
