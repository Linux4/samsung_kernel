/*
 * Copyright (C) 2017 The Android Open Source Project
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

import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.avrcp.AvrcpTargetService;
import com.android.bluetooth.hearingaid.HearingAidService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hid.HidDeviceService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.pan.PanService;

import android.util.Log;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

// Factory class to create instances of static services. Useful in mocking the service objects.
public class ServiceFactory {
    private static final String TAG = "BluetoothServiceFactory";

    Object mGroupService = null;

    public A2dpService getA2dpService() {
        return A2dpService.getA2dpService();
    }

    public HeadsetService getHeadsetService() {
        return HeadsetService.getHeadsetService();
    }

    public HidHostService getHidHostService() {
        return HidHostService.getHidHostService();
    }

    public HidDeviceService getHidDeviceService() {
        return HidDeviceService.getHidDeviceService();
    }

    public PanService getPanService() {
        return PanService.getPanService();
    }

    public HearingAidService getHearingAidService() {
        return HearingAidService.getHearingAidService();
    }

    public A2dpSinkService getA2dpSinkService() {
        return A2dpSinkService.getA2dpSinkService();
    }

    public AvrcpTargetService getAvrcpTargetService() {
        return AvrcpTargetService.get();
    }

    public Object getGroupService() {
        if (mGroupService == null) {
            Method mGetGroupService = null;
            try {
                Class<?> grpSvcCls =
                        Class.forName("com.android.bluetooth.groupclient.GroupService");
                if (grpSvcCls != null) {
                    mGetGroupService = grpSvcCls.getMethod("getGroupService");
                    if (mGetGroupService != null) {
                        mGroupService = mGetGroupService.invoke(null);
                    }
                }
            } catch (NoSuchMethodException|IllegalAccessException|
                     InvocationTargetException|ClassNotFoundException e) {
                 Log.e(TAG, "Exception in getGroupService: " + e);
            }
        }
        return mGroupService;
    }

}
