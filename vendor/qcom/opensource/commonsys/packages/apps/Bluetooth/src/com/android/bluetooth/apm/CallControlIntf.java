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

public class CallControlIntf {
    public static final String TAG = "APM: CallControlIntf";
    private static CallControlIntf mInterface = null;

    static Class CallControl = null;
    static Object mCallControlObj = null;

    private CallControlIntf() {

    }

     public static CallControlIntf get() {
        if(mInterface == null) {
            mInterface = new CallControlIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");
        mCallControlObj = obj;

        try {
            CallControl = Class.forName("com.android.bluetooth.apm.CallControl");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class CallControl not present. " + ex);
            CallControl = null;
            mCallControlObj = null;
        }
    }

    public boolean init(Context context) {
        if(CallControl == null)
            return false;

        Class[] arg = new Class[1];
        arg[0] = Context.class;

        try {
            Method init = CallControl.getDeclaredMethod("init", arg);
            Boolean ret = (Boolean)init.invoke(mCallControlObj, context);
            return ret;
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "init Exception" + e);
        }
        return false;
    }

    public void phoneStateChanged(int numActive, int numHeld, int callState, String number,
            int type, String name, boolean isVirtualCall) {
        if(CallControl == null)
            return;
        Log.i(TAG, "phoneStateChanged");

        Class[] arg = new Class[7];
        arg[0] = Integer.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;
        arg[3] = String.class;
        arg[4] = Integer.class;
        arg[5] = String.class;
        arg[6] = Boolean.class;

        try {
            Method phoneStateChanged = CallControl.getDeclaredMethod("phoneStateChanged", arg);
            phoneStateChanged.invoke(mCallControlObj, numActive, numHeld, callState, number, type, name, isVirtualCall);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "phoneStateChanged Exception" + e);
        }
    }

   public void clccResponse(int index, int direction, int status, int mode, boolean mpty,
                String number, int type) {
       if(CallControl == null)
            return;
       Log.i(TAG, "clccResponse");

        Class[] arg = new Class[7];
        arg[0] = Integer.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;
        arg[3] = Integer.class;
        arg[4] = Boolean.class;
        arg[5] = String.class;
        arg[6] = Integer.class;

        try {
            Method clccResponse = CallControl.getDeclaredMethod("clccResponse", arg);
            clccResponse.invoke(mCallControlObj, index, direction, status, mode, mpty, number, type);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "clccResponse Exception" + e);
        }
    }

    public void updateBearerName(String operatorStr) {
        if(CallControl == null)
            return;
        Log.i(TAG, "updateBearerName");

        Class[] arg = new Class[1];
        arg[0] = String.class;

        try {
            Method updateBearerName = CallControl.getDeclaredMethod("updateBearerName", arg);
            updateBearerName.invoke(mCallControlObj, operatorStr);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "updateBearerName Exception" + e);
        }
    }

    public void updateBearerTechnology(int bearer_tech) {
        if(CallControl == null)
            return;
        Log.i(TAG, "updateBearerTechnology");

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method updateBearerTechnology = CallControl.getDeclaredMethod("updateBearerTechnology", arg);
            updateBearerTechnology.invoke(mCallControlObj, bearer_tech);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "updateBearerTechnology Exception" + e);
        }
    }

    public void updateOriginateResult(BluetoothDevice device, int event, int res) {
        if(CallControl == null)
            return;
        Log.i(TAG, "updateOriginateResult");

        Class[] arg = new Class[3];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;
        arg[2] = Integer.class;

        try {
            Method originateResult = CallControl.getDeclaredMethod("updateOriginateResult", arg);
            originateResult.invoke(mCallControlObj, device, event, res);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "updateOriginateResult: Exception" + e);
        }
    }
    public void updateSignalStatus(int signal) {
        if(CallControl == null)
            return;
        Log.i(TAG, "updateSignalStatus");

        Class[] arg = new Class[1];
        arg[0] = Integer.class;

        try {
            Method updateSignalStatus = CallControl.getDeclaredMethod("updateSignalStatus", arg);
            updateSignalStatus.invoke(mCallControlObj, signal);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "updateSignalStatus Exception" + e);
        }
    }

    public void answerCall (BluetoothDevice device) {
        if(CallControl == null)
            return;
        Log.i(TAG, "answerCall");

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method answerCall = CallControl.getDeclaredMethod("answerCall", arg);
            answerCall.invoke(mCallControlObj, device);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "answerCall Exception" + e);
        }
    }

    public void hangupCall (BluetoothDevice device) {
        if(CallControl == null)
            return;
        Log.i(TAG, "hangupCall");

        Class[] arg = new Class[1];
        arg[0] = BluetoothDevice.class;

        try {
            Method hangupCall = CallControl.getDeclaredMethod("hangupCall", arg);
            hangupCall.invoke(mCallControlObj, device);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "hangupCall Exception" + e);
        }
    }

    public boolean processChld (BluetoothDevice device, int chld) {
        if(CallControl == null)
            return false;
        Log.i(TAG, "processChld");

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = Integer.class;

        try {
            Method processChld = CallControl.getDeclaredMethod("processChld", arg);
            Boolean ret = (boolean)processChld.invoke(mCallControlObj, device, chld);
            return ret;
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "processChld Exception" + e);
        }
        return false;
    }

    public void dial (BluetoothDevice device, String dialNumber) {
        if(CallControl == null)
            return;
        Log.i(TAG, "dial");

        Class[] arg = new Class[2];
        arg[0] = BluetoothDevice.class;
        arg[1] = String.class;

        try {
            Method dial = CallControl.getDeclaredMethod("dial", arg);
            dial.invoke(mCallControlObj, device, dialNumber);
        } catch(IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            Log.i(TAG, "dial Exception" + e);
         }
    }
}
