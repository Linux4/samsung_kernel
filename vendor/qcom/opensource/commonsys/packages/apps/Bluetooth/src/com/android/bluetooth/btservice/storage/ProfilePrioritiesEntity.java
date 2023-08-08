/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.btservice.storage;

import android.bluetooth.BluetoothProfile;

import androidx.room.Entity;

@Entity
class ProfilePrioritiesEntity {
    /* Bluetooth profile priorities*/
    public int a2dp_connection_policy;
    public int a2dp_sink_connection_policy;
    public int hfp_connection_policy;
    public int hfp_client_connection_policy;
    public int hid_host_connection_policy;
    public int pan_connection_policy;
    public int pbap_connection_policy;
    public int pbap_client_connection_policy;
    public int map_connection_policy;
    public int sap_connection_policy;
    public int hearing_aid_connection_policy;
    public int map_client_connection_policy;
    public int bc_profile_priority;
    public int csip_set_coordinator_connection_policy;
    public int le_audio_connection_policy;
    public int bass_client_connection_policy;
    public int volume_control_connection_policy;

    ProfilePrioritiesEntity() {
        a2dp_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        a2dp_sink_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        hfp_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        hfp_client_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        hid_host_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        pan_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        pbap_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        pbap_client_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        map_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        sap_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        hearing_aid_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        map_client_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        bc_profile_priority = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        csip_set_coordinator_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        le_audio_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        bass_client_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        volume_control_connection_policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
    }

    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("A2DP=").append(a2dp_connection_policy)
                .append("|A2DP_SINK=").append(a2dp_sink_connection_policy)
                .append("|CSIP_SET_COORDINATOR=").append(csip_set_coordinator_connection_policy)
                .append("|HEADSET=").append(hfp_connection_policy)
                .append("|HEADSET_CLIENT=").append(hfp_client_connection_policy)
                .append("|HID_HOST=").append(hid_host_connection_policy)
                .append("|PAN=").append(pan_connection_policy)
                .append("|PBAP=").append(pbap_connection_policy)
                .append("|PBAP_CLIENT=").append(pbap_client_connection_policy)
                .append("|MAP=").append(map_connection_policy)
                .append("|MAP_CLIENT=").append(map_client_connection_policy)
                .append("|SAP=").append(sap_connection_policy)
                .append("|HEARING_AID=").append(hearing_aid_connection_policy)
                .append("|LE_AUDIO=").append(le_audio_connection_policy)
                .append("|LE_BROADCAST_ASSISTANT=").append(bass_client_connection_policy)
                .append("|VOLUME_CONTROL=").append(volume_control_connection_policy);

        return builder.toString();
    }
}
