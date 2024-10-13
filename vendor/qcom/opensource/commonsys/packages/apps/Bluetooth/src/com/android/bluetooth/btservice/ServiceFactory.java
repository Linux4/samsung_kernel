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

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

package com.android.bluetooth.btservice;

import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.avrcp.AvrcpTargetService;
import com.android.bluetooth.csip.CsipSetCoordinatorService;
import com.android.bluetooth.groupclient.GroupService;
import com.android.bluetooth.hearingaid.HearingAidService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hid.HidDeviceService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.lebroadcast.BassClientService;
import com.android.bluetooth.pan.PanService;
import com.android.bluetooth.le_audio.LeAudioService;
import com.android.bluetooth.vc.VolumeControlService;

import android.util.Log;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

// Factory class to create instances of static services. Useful in mocking the service objects.
public class ServiceFactory {
    private static final String TAG = "BluetoothServiceFactory";

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

    public GroupService getGroupService() {
        return GroupService.getGroupService();
    }

    public CsipSetCoordinatorService getCsipSetCoordinatorService() {
        return CsipSetCoordinatorService.getCsipSetCoordinatorService();
    }

    public LeAudioService getLeAudioService() {
        return LeAudioService.getLeAudioService();
    }

    public BassClientService getBassClientService() {
        return BassClientService.getBassClientService();
    }

    public VolumeControlService getVolumeControlService() {
        return VolumeControlService.getVolumeControlService();
    }
}
