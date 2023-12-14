/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

package com.android.bluetooth.directionfinder;

import android.os.Binder;
import android.os.UserHandle;

import java.util.List;
import java.util.Objects;

import android.bluetooth.BluetoothDevice;

/**
 * Helper class for saving enable Ble Direction Finding parameters.
 *
 * @hide
 */
/* package */class AtpEnableDirFinding {
    public BluetoothDevice device;
    public int samplingEnable;
    public int slotDurations;
    public int enable;
    public int cteReqInt;
    public int reqCteLen;
    public int dirFindingType;

    AtpEnableDirFinding(BluetoothDevice device, int samplingEnable, int slotDurations,
            int enable, int cteReqInt, int reqCteLen, int dirFindingType) {
        this.device = device;
        this.samplingEnable = samplingEnable;
        this.slotDurations = slotDurations;
        this.enable = enable;
        this.cteReqInt = cteReqInt;
        this.reqCteLen = reqCteLen;
        this.dirFindingType = dirFindingType;
    }
}
