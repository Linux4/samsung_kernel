/*
 * Copyright (C) 2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 * * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

/*
 * Copyright (C) 2012 The Android Open Source Project
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

/*
 * @hide
 */

public final class AbstractionLayer {
    // Do not modify without upating the HAL files.

    // TODO: Some of the constants are repeated from BluetoothAdapter.java.
    // Get rid of them and maintain just one.
    static final int BT_STATE_OFF = 0x00;
    static final int BT_STATE_ON = 0x01;

    static final int BT_SCAN_MODE_NONE = 0x00;
    static final int BT_SCAN_MODE_CONNECTABLE = 0x01;
    static final int BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE = 0x02;

    static final int BT_PROPERTY_BDNAME = 0x01;
    static final int BT_PROPERTY_BDADDR = 0x02;
    static final int BT_PROPERTY_UUIDS = 0x03;
    static final int BT_PROPERTY_CLASS_OF_DEVICE = 0x04;
    static final int BT_PROPERTY_TYPE_OF_DEVICE = 0x05;
    static final int BT_PROPERTY_SERVICE_RECORD = 0x06;
    static final int BT_PROPERTY_ADAPTER_SCAN_MODE = 0x07;
    static final int BT_PROPERTY_ADAPTER_BONDED_DEVICES = 0x08;
    static final int BT_PROPERTY_ADAPTER_DISCOVERABLE_TIMEOUT = 0x09;

    static final int BT_PROPERTY_REMOTE_FRIENDLY_NAME = 0x0A;
    static final int BT_PROPERTY_REMOTE_RSSI = 0x0B;

    static final int BT_PROPERTY_REMOTE_IS_COORDINATED_SET_MEMBER = 0x11;

    static final int BT_PROPERTY_REMOTE_VERSION_INFO = 0x0C;
    static final int BT_PROPERTY_LOCAL_LE_FEATURES = 0x0D;

    static final int BT_PROPERTY_DYNAMIC_AUDIO_BUFFER = 0x10;
    static final int BT_PROPERTY_REMOTE_DEVICE_GROUP = 0x12;

    public static final int BT_DEVICE_TYPE_BREDR = 0x01;
    public static final int BT_DEVICE_TYPE_BLE = 0x02;
    public static final int BT_DEVICE_TYPE_DUAL = 0x03;

    static final int BT_PROPERTY_LOCAL_IO_CAPS = 0x0e;
    static final int BT_PROPERTY_LOCAL_IO_CAPS_BLE = 0x0f;

    static final int BT_PROPERTY_ADV_AUDIO_UUIDS = 0xA0;
    static final int BT_PROPERTY_ADV_AUDIO_ACTION_UUID = 0xA1;
    static final int BT_PROPERTY_ADV_AUDIO_UUID_TRANSPORT = 0xA2;
    static final int BT_PROPERTY_ADV_AUDIO_VALID_ADDR_TYPE = 0xA3;
    static final int BT_PROPERTY_REM_DEV_IDENT_BD_ADDR = 0xA4;
    static final int BT_PROPERTY_ADV_AUDIO_UUID_BY_TRANSPORT = 0xA5;

    static final int BT_PROPERTY_GROUP_EIR_DATA = 0xFE;

    static final int BT_BOND_STATE_NONE = 0x00;
    static final int BT_BOND_STATE_BONDING = 0x01;
    static final int BT_BOND_STATE_BONDED = 0x02;

    static final int BT_SSP_VARIANT_PASSKEY_CONFIRMATION = 0x00;
    static final int BT_SSP_VARIANT_PASSKEY_ENTRY = 0x01;
    static final int BT_SSP_VARIANT_CONSENT = 0x02;
    static final int BT_SSP_VARIANT_PASSKEY_NOTIFICATION = 0x03;

    static final int BT_DISCOVERY_STOPPED = 0x00;
    static final int BT_DISCOVERY_STARTED = 0x01;

    static final int BT_ACL_STATE_CONNECTED = 0x00;
    static final int BT_ACL_STATE_DISCONNECTED = 0x01;

    static final int BT_UUID_SIZE = 16; // bytes

    public static final int BT_STATUS_SUCCESS = 0;
    public static final int BT_STATUS_FAIL = 1;
    public static final int BT_STATUS_NOT_READY = 2;
    public static final int BT_STATUS_NOMEM = 3;
    public static final int BT_STATUS_BUSY = 4;
    public static final int BT_STATUS_DONE = 5;
    public static final int BT_STATUS_UNSUPPORTED = 6;
    public static final int BT_STATUS_PARM_INVALID = 7;
    public static final int BT_STATUS_UNHANDLED = 8;
    public static final int BT_STATUS_AUTH_FAILURE = 9;
    public static final int BT_STATUS_RMT_DEV_DOWN = 10;
    public static final int BT_STATUS_AUTH_REJECTED = 11;
    public static final int BT_STATUS_AUTH_TIMEOUT = 12;

    // Profile IDs to get profile features from profile_conf
    public static final int AVRCP = 1;
    public static final int PBAP = 2;
    public static final int MAP = 3;
    public static final int MAX_POW = 4;
    public static final int OPP = 5;
    public static final int RF_PATH_LOSS = 6;

    // Profile features supported in profile_conf
    public static final int PROFILE_VERSION =1;
    public static final int AVRCP_COVERART_SUPPORT = 2;
    public static final int AVRCP_0103_SUPPORT = 3;
    public static final int USE_SIM_SUPPORT = 4;
    public static final int MAP_EMAIL_SUPPORT = 5;
    public static final int PBAP_0102_SUPPORT = 6;
    public static final int MAP_0104_SUPPORT = 7;
    public static final int BR_MAX_POW_SUPPORT = 8;
    public static final int EDR_MAX_POW_SUPPORT = 9;
    public static final int BLE_MAX_POW_SUPPORT = 10;
    public static final int OPP_0100_SUPPORT = 11;
    public static final int RF_TX_PATH_COMPENSATION_VALUE = 12;
    public static final int RF_RX_PATH_COMPENSATION_VALUE = 13;


    static final int BT_VENDOR_PROPERTY_TWS_PLUS_DEVICE_TYPE = 0x01;
    static final int BT_VENDOR_PROPERTY_TWS_PLUS_PEER_ADDR = 0x02;
    static final int BT_VENDOR_PROPERTY_TWS_PLUS_AUTO_CONNECT = 0x03;
    static final int BT_VENDOR_PROPERTY_HOST_ADD_ON_FEATURES = 0x04;
    static final int BT_VENDOR_PROPERTY_SOC_ADD_ON_FEATURES = 0x05;
    static final int BT_VENDOR_PROPERTY_WL_MEDIA_PLAYERS_LIST = 0x06;
    static final int TWS_PLUS_DEV_TYPE_NONE = 0x00;
    static final int TWS_PLUS_DEV_TYPE_PRIMARY = 0x01;
    static final int TWS_PLUS_DEV_TYPE_SECONDARY = 0x02;
}
