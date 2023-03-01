/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.bluetooth.hfp;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.Context;

import java.util.List;

/**
 * A proxy class that facilitates testing of the BluetoothInCallService class.
 *
 * This is necessary due to the "final" attribute of the BluetoothHeadset class. In order to
 * test the correct functioning of the BluetoothInCallService class, the final class must be put
 * into a container that can be mocked correctly.
 */
public class BluetoothHeadsetProxy {

    private BluetoothHeadset mBluetoothHeadset;

    public BluetoothHeadsetProxy(BluetoothHeadset headset) {
        mBluetoothHeadset = headset;
    }
    public void closeBluetoothHeadsetProxy(Context context) {
         final BluetoothManager btManager =
                 context.getSystemService(BluetoothManager.class);
         if (btManager != null) {
             btManager.getAdapter().closeProfileProxy(BluetoothProfile.HEADSET, mBluetoothHeadset);
         }
    }

    public void clccResponse(int index, int direction, int status, int mode, boolean mpty,
            String number, int type) {

        mBluetoothHeadset.clccResponse(index, direction, status, mode, mpty, number, type);
    }

    public void phoneStateChanged(int numActive, int numHeld, int callState, String number,
            int type, String name) {

        mBluetoothHeadset.phoneStateChanged(numActive, numHeld, callState, number, type,
                name);
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return mBluetoothHeadset.getConnectedDevices();
    }

    public int getConnectionState(BluetoothDevice device) {
        return mBluetoothHeadset.getConnectionState(device);
    }

    public int getAudioState(BluetoothDevice device) {
        return mBluetoothHeadset.getAudioState(device);
    }

    public int connectAudio() {
        return mBluetoothHeadset.connectAudio();
    }

    public boolean setActiveDevice(BluetoothDevice device) {
        return mBluetoothHeadset.setActiveDevice(device);
    }

    public BluetoothDevice getActiveDevice() {
        return mBluetoothHeadset.getActiveDevice();
    }

    public boolean isAudioOn() {
        int scoConnectionRequest = mBluetoothHeadset.connectAudio();
        return scoConnectionRequest == BluetoothStatusCodes.SUCCESS ||
            scoConnectionRequest == BluetoothStatusCodes.ERROR_AUDIO_DEVICE_ALREADY_CONNECTED;
    }

    public int disconnectAudio() {
        return mBluetoothHeadset.disconnectAudio();
    }

    public boolean isInbandRingingEnabled() {
        return mBluetoothHeadset.isInbandRingingEnabled();
    }
}
