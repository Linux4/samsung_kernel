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
    }

    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("A2DP=").append(a2dp_connection_policy)
                .append("|A2DP_SINK=").append(a2dp_sink_connection_policy)
                .append("|HEADSET=").append(hfp_connection_policy)
                .append("|HEADSET_CLIENT=").append(hfp_client_connection_policy)
                .append("|HID_HOST=").append(hid_host_connection_policy)
                .append("|PAN=").append(pan_connection_policy)
                .append("|PBAP=").append(pbap_connection_policy)
                .append("|PBAP_CLIENT=").append(pbap_client_connection_policy)
                .append("|MAP=").append(map_connection_policy)
                .append("|MAP_CLIENT=").append(map_client_connection_policy)
                .append("|SAP=").append(sap_connection_policy)
                .append("|HEARING_AID=").append(hearing_aid_connection_policy);

        return builder.toString();
    }
}
