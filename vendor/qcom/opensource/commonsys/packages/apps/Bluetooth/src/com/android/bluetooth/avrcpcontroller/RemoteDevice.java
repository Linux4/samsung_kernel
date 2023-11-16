/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.bluetooth.BluetoothDevice;

import com.android.bluetooth.Utils;

/*
 * Contains information about remote device specifically the player and features enabled on it along
 * with an encapsulation of the current track and playlist information.
 */
class RemoteDevice {

    /*
     * Remote features from JNI
     */
    private static final int FEAT_NONE = 0;
    private static final int FEAT_METADATA = 1;
    private static final int FEAT_ABSOLUTE_VOLUME = 2;
    private static final int FEAT_BROWSE = 4;
    private static final int FEAT_COVER_ART = 8;

    private static final int VOLUME_LABEL_UNDEFINED = -1;
    private static final int L2CAP_PSM_UNDEFINED = -1;

    final BluetoothDevice mBTDevice;
    private int mRemoteFeatures;
    private boolean mAbsVolNotificationRequested;
    private boolean mFirstAbsVolCmdRecvd;
    private int mNotificationLabel;
    private int mBipL2capPsm;

    RemoteDevice(BluetoothDevice mDevice) {
        mBTDevice = mDevice;
        mRemoteFeatures = FEAT_NONE;
        mAbsVolNotificationRequested = false;
        mNotificationLabel = VOLUME_LABEL_UNDEFINED;
        mFirstAbsVolCmdRecvd = false;
        mBipL2capPsm = L2CAP_PSM_UNDEFINED;
    }

    synchronized void setRemoteFeatures(int remoteFeatures) {
        mRemoteFeatures = remoteFeatures;
    }

    synchronized void setRemoteBipPsm( int remotePsm) {
        mBipL2capPsm = remotePsm;
    }

    synchronized int getRemoteBipPsm () {
        return mBipL2capPsm;
    }

    synchronized boolean isBrowsingSupported() {
        return ((mRemoteFeatures & FEAT_BROWSE) != 0);
    }

    synchronized boolean isMetaDataSupported() {
        return ((mRemoteFeatures & FEAT_METADATA) != 0);
    }

    synchronized boolean isCoverArtSupported() {
        return ((mRemoteFeatures & FEAT_COVER_ART) != 0);
    }

    synchronized public byte[] getBluetoothAddress() {
        return Utils.getByteAddress(mBTDevice);
    }

    public synchronized void setNotificationLabel(int label) {
        mNotificationLabel = label;
    }

    public synchronized int getNotificationLabel() {
        return mNotificationLabel;
    }

    public synchronized void setAbsVolNotificationRequested(boolean request) {
        mAbsVolNotificationRequested = request;
    }

    public synchronized boolean getAbsVolNotificationRequested() {
        return mAbsVolNotificationRequested;
    }

    public synchronized void setFirstAbsVolCmdRecvd() {
        mFirstAbsVolCmdRecvd = true;
    }

    public synchronized boolean getFirstAbsVolCmdRecvd() {
        return mFirstAbsVolCmdRecvd;
    }
}
