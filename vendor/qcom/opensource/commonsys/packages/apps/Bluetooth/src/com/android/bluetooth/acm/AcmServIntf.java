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

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials provided
       with the distribution.
     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.
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
 **************************************************************************/

package com.android.bluetooth.acm;

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
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
import com.android.internal.annotations.VisibleForTesting;

public class AcmServIntf {
    public static final String TAG = "ACM: AcmServIntf";
    private static AcmServIntf mInterface = null;

    static Class AcmService = null;
    static Object mAcmService = null;

    private AcmServIntf() {

    }

    public static AcmServIntf get() {
        if(mInterface == null) {
            mInterface = new AcmServIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mAcmService = obj;

        try {
            AcmService = Class.forName("com.android.bluetooth.acm.AcmService");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class AcmService not present. " + ex);
            AcmService = null;
            mAcmService = null;
        }
    }

    public boolean connect(BluetoothDevice device) {
        if(AcmService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method connect = AcmService.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mAcmService, device);
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

/*
    public boolean connect(BluetoothDevice device, boolean allProfile) {
        if(AcmService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method connect = AcmService.getDeclaredMethod("connect", arg);
            Boolean ret = (Boolean)connect.invoke(mAcmService, device, allProfile);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    } */

    public boolean autoConnect(BluetoothDevice device) {
        if(AcmService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method autoConnect = AcmService.getDeclaredMethod("autoConnect", arg);
            Boolean ret = (Boolean)autoConnect.invoke(mAcmService, device);
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
        if(AcmService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disconnect = AcmService.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mAcmService, device);
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

/*
    public boolean disconnect(BluetoothDevice device, boolean allProfile) {
        if(AcmService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method disconnect = AcmService.getDeclaredMethod("disconnect", arg);
            Boolean ret = (Boolean)disconnect.invoke(mAcmService, device, allProfile);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    } */

    public List<BluetoothDevice> getConnectedDevices() {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(AcmService == null)
            return devices;

        try {
            Method getConnectedDevices = AcmService.getDeclaredMethod("getConnectedDevices");
            List<BluetoothDevice>  ret = (List<BluetoothDevice>)getConnectedDevices.invoke(mAcmService);
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
        if(AcmService == null)
            return devices;

        Class[] arg = new Class[1];
        arg[0] = int[].class;

        try {
            Method getDevicesMatchingConnectionStates =
                    AcmService.getDeclaredMethod("getDevicesMatchingConnectionStates", arg);
            List<BluetoothDevice>  ret =
                    (List<BluetoothDevice>)getDevicesMatchingConnectionStates.invoke(mAcmService, states);
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

    public List<BluetoothDevice> getDevices() {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if(AcmService == null)
            return devices;

        try {
            Method getDevices = AcmService.getDeclaredMethod("getDevices");
            List<BluetoothDevice>  ret = (List<BluetoothDevice>)getDevices.invoke(mAcmService);
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
        if(AcmService == null)
            return BluetoothProfile.STATE_DISCONNECTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionState = AcmService.getDeclaredMethod("getConnectionState", arg);
            int ret = (int)getConnectionState.invoke(mAcmService, device);
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
        if(AcmService == null)
            return BluetoothProfile.PRIORITY_UNDEFINED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getPriority = AcmService.getDeclaredMethod("getPriority", arg);
            int ret = (int)getPriority.invoke(mAcmService, device);
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
        if(AcmService == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method isA2dpPlaying = AcmService.getDeclaredMethod("isA2dpPlaying", arg);
            boolean ret = (boolean)isA2dpPlaying.invoke(mAcmService, device);
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

/*
    public BluetoothCodecStatus getCodecStatus(BluetoothDevice device) {
        if(AcmService == null)
            return null;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getCodecStatus = AcmService.getDeclaredMethod("getCodecStatus", arg);
            BluetoothCodecStatus ret = (BluetoothCodecStatus)getCodecStatus.invoke(mAcmService, device);
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
        if(AcmService == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecConfig.class;

        try {
            Method setCodecConfigPreference = AcmService.getDeclaredMethod("setCodecConfigPreference", arg);
            setCodecConfigPreference.invoke(mAcmService, device, codecConfig);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void enableOptionalCodecs(BluetoothDevice device) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method enableOptionalCodecs = AcmService.getDeclaredMethod("enableOptionalCodecs", arg);
            enableOptionalCodecs.invoke(mAcmService, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void getSupportsOptionalCodecs(BluetoothDevice device) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getSupportsOptionalCodecs = AcmService.getDeclaredMethod("getSupportsOptionalCodecs", arg);
            getSupportsOptionalCodecs.invoke(mAcmService, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int supportsOptionalCodecs(BluetoothDevice device) {
        if(AcmService == null)
            return BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method supportsOptionalCodecs = AcmService.getDeclaredMethod("supportsOptionalCodecs", arg);
            Integer ret = (Integer)supportsOptionalCodecs.invoke(mAcmService, device);
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
        if(AcmService == null)
            return BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getOptionalCodecsEnabled = AcmService.getDeclaredMethod("getOptionalCodecsEnabled", arg);
            Integer ret = (Integer)getOptionalCodecsEnabled.invoke(mAcmService, device);
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
        if(AcmService == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method disableOptionalCodecs = AcmService.getDeclaredMethod("disableOptionalCodecs", arg);
            disableOptionalCodecs.invoke(mAcmService, device);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void setOptionalCodecsEnabled(BluetoothDevice device, int value) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setOptionalCodecsEnabled = AcmService.getDeclaredMethod("setOptionalCodecsEnabled", arg);
            setOptionalCodecsEnabled.invoke(mAcmService, device, value);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }*/

    @VisibleForTesting
    public void bondStateChanged(BluetoothDevice device, int bondState) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method bondStateChanged = AcmService.getDeclaredMethod("bondStateChanged", arg);
            bondStateChanged.invoke(mAcmService, device, bondState);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void removeStateMachine(BluetoothDevice device) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method removeStateMachine = AcmService.getDeclaredMethod("removeStateMachine", arg);
            removeStateMachine.invoke(mAcmService, device);
            return;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public int getConnectionPolicy(BluetoothDevice device) {
        if(AcmService == null)
            return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method getConnectionPolicy = AcmService.getDeclaredMethod("getConnectionPolicy", arg);
            int ret = (int)getConnectionPolicy.invoke(mAcmService, device);
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
        if(AcmService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method setConnectionPolicy = AcmService.getDeclaredMethod("setConnectionPolicy", arg);
            boolean ret = (boolean)setConnectionPolicy.invoke(mAcmService, device, connectionPolicy);
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

/*
    public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        if(AcmService == null)
            return false;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Boolean.class;

        try {
            Method setSilenceMode = AcmService.getDeclaredMethod("setSilenceMode", arg);
            boolean ret = (boolean)setSilenceMode.invoke(mAcmService, device, silence);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
        return false;
    }*/

    public void onConnStateChange(BluetoothDevice device, int state, int profile) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method onConnStateChange = AcmService.getDeclaredMethod("onConnStateChange", arg);
            onConnStateChange.invoke(mAcmService, device, state, profile);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onStreamStateChange(BluetoothDevice device, int streamStatus) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method onStreamStateChange = AcmService.getDeclaredMethod("onStreamStateChange", arg);
            onStreamStateChange.invoke(mAcmService, device, streamStatus);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

/*
    public void onCodecConfigChange(BluetoothDevice device, BluetoothCodecStatus mCodecStatus, int profile) {
        if(AcmService == null)
            return;

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecStatus.class;
        arg[2] = Integer.class;

        try {
            Method onCodecConfigChange = AcmService.getDeclaredMethod("onCodecConfigChange", arg);
            onCodecConfigChange.invoke(mAcmService, device, mCodecStatus, profile);
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
        if(AcmService == null)
            return;

        Class[] arg = new Class[4];
        arg[0] = BluetoothDevice.class;
        arg[1] = BluetoothCodecStatus.class;
        arg[2] = Integer.class;
        arg[3] = Boolean.class;

        try {
            Method onCodecConfigChange = AcmService.getDeclaredMethod("onCodecConfigChange", arg);
            onCodecConfigChange.invoke(mAcmService, device, mCodecStatus, profile, updateAudio);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }*/
}
