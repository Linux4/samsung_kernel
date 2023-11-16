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

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProtoEnums;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Binder;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.provider.Settings;
import android.util.Log;

import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * The active device manager is responsible to handle a Room database
 * for Bluetooth persistent data.
 */
public class DatabaseManager {
    private static final boolean DBG = true;
    private static final boolean VERBOSE = true;
    private static final String TAG = "BluetoothDatabase";

    private AdapterService mAdapterService = null;
    private HandlerThread mHandlerThread = null;
    private Handler mHandler = null;
    private MetadataDatabase mDatabase = null;
    private boolean mMigratedFromSettingsGlobal = false;

    @VisibleForTesting
    final Map<String, Metadata> mMetadataCache = new HashMap<>();
    private final Semaphore mSemaphore = new Semaphore(1);

    private static final int LOAD_DATABASE_TIMEOUT = 500; // milliseconds
    private static final int MSG_LOAD_DATABASE = 0;
    private static final int MSG_UPDATE_DATABASE = 1;
    private static final int MSG_DELETE_DATABASE = 2;
    private static final int MSG_CLEAR_DATABASE = 100;
    private static final String LOCAL_STORAGE = "LocalStorage";

    private static final String
            LEGACY_BTSNOOP_DEFAULT_MODE = "bluetooth_btsnoop_default_mode";
    private static final String
            LEGACY_HEADSET_PRIORITY_PREFIX = "bluetooth_headset_priority_";
    private static final String
            LEGACY_A2DP_SINK_PRIORITY_PREFIX = "bluetooth_a2dp_sink_priority_";
    private static final String
            LEGACY_A2DP_SRC_PRIORITY_PREFIX = "bluetooth_a2dp_src_priority_";
    private static final String LEGACY_A2DP_SUPPORTS_OPTIONAL_CODECS_PREFIX =
            "bluetooth_a2dp_supports_optional_codecs_";
    private static final String LEGACY_A2DP_OPTIONAL_CODECS_ENABLED_PREFIX =
            "bluetooth_a2dp_optional_codecs_enabled_";
    private static final String
            LEGACY_INPUT_DEVICE_PRIORITY_PREFIX = "bluetooth_input_device_priority_";
    private static final String
            LEGACY_MAP_PRIORITY_PREFIX = "bluetooth_map_priority_";
    private static final String
            LEGACY_MAP_CLIENT_PRIORITY_PREFIX = "bluetooth_map_client_priority_";
    private static final String
            LEGACY_PBAP_CLIENT_PRIORITY_PREFIX = "bluetooth_pbap_client_priority_";
    private static final String
            LEGACY_SAP_PRIORITY_PREFIX = "bluetooth_sap_priority_";
    private static final String
            LEGACY_PAN_PRIORITY_PREFIX = "bluetooth_pan_priority_";
    private static final String
            LEGACY_HEARING_AID_PRIORITY_PREFIX = "bluetooth_hearing_aid_priority_";

    /**
     * Constructor of the DatabaseManager
     */
    public DatabaseManager(AdapterService service) {
        mAdapterService = service;
    }

    class DatabaseHandler extends Handler {
        DatabaseHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_LOAD_DATABASE: {
                    synchronized (mDatabase) {
                        List<Metadata> list;
                        try {
                            list = mDatabase.load();
                        } catch (IllegalStateException e) {
                            Log.e(TAG, "Unable to open database: " + e);
                            mDatabase = MetadataDatabase
                                    .createDatabaseWithoutMigration(mAdapterService);
                            list = mDatabase.load();
                        }
                        compactLastConnectionTime(list);
                        cacheMetadata(list);
                    }
                    break;
                }
                case MSG_UPDATE_DATABASE: {
                    Metadata data = (Metadata) msg.obj;
                    synchronized (mDatabase) {
                        mDatabase.insert(data);
                    }
                    break;
                }
                case MSG_DELETE_DATABASE: {
                    String address = (String) msg.obj;
                    synchronized (mDatabase) {
                        mDatabase.delete(address);
                    }
                    break;
                }
                case MSG_CLEAR_DATABASE: {
                    synchronized (mDatabase) {
                        mDatabase.deleteAll();
                    }
                    break;
                }
            }
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.e(TAG, "Received intent with null action");
                return;
            }
            switch (action) {
                case BluetoothDevice.ACTION_BOND_STATE_CHANGED: {
                    int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                            BluetoothDevice.ERROR);
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    Objects.requireNonNull(device,
                            "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
                    bondStateChanged(device, state);
                    break;
                }
                case BluetoothAdapter.ACTION_STATE_CHANGED: {
                    int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE,
                            BluetoothAdapter.STATE_OFF);
                    if (!mMigratedFromSettingsGlobal
                            && state == BluetoothAdapter.STATE_TURNING_ON) {
                        migrateSettingsGlobal();
                    }
                    break;
                }
            }
        }
    };

    void bondStateChanged(BluetoothDevice device, int state) {
        synchronized (mMetadataCache) {
            String address = device.getAddress();
            if (state != BluetoothDevice.BOND_NONE) {
                if (mMetadataCache.containsKey(address)) {
                    return;
                }
                createMetadata(address, false);
            } else {
                Metadata metadata = mMetadataCache.get(address);
                if (metadata != null) {
                    mMetadataCache.remove(address);
                    deleteDatabase(metadata);
                }
            }
        }
    }

    boolean isValidMetaKey(int key) {
        switch (key) {
            case BluetoothDevice.METADATA_MANUFACTURER_NAME:
            case BluetoothDevice.METADATA_MODEL_NAME:
            case BluetoothDevice.METADATA_SOFTWARE_VERSION:
            case BluetoothDevice.METADATA_HARDWARE_VERSION:
            case BluetoothDevice.METADATA_COMPANION_APP:
            case BluetoothDevice.METADATA_MAIN_ICON:
            case BluetoothDevice.METADATA_IS_UNTETHERED_HEADSET:
            case BluetoothDevice.METADATA_UNTETHERED_LEFT_ICON:
            case BluetoothDevice.METADATA_UNTETHERED_RIGHT_ICON:
            case BluetoothDevice.METADATA_UNTETHERED_CASE_ICON:
            case BluetoothDevice.METADATA_UNTETHERED_LEFT_BATTERY:
            case BluetoothDevice.METADATA_UNTETHERED_RIGHT_BATTERY:
            case BluetoothDevice.METADATA_UNTETHERED_CASE_BATTERY:
            case BluetoothDevice.METADATA_UNTETHERED_LEFT_CHARGING:
            case BluetoothDevice.METADATA_UNTETHERED_RIGHT_CHARGING:
            case BluetoothDevice.METADATA_UNTETHERED_CASE_CHARGING:
            case BluetoothDevice.METADATA_ENHANCED_SETTINGS_UI_URI:
                return true;
        }
        Log.w(TAG, "Invalid metadata key " + key);
        return false;
    }

    /**
     * Set customized metadata to database with requested key
     */
    @VisibleForTesting
    public boolean setCustomMeta(BluetoothDevice device, int key, byte[] newValue) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setCustomMeta: device is null");
                return false;
            }
            if (!isValidMetaKey(key)) {
                Log.e(TAG, "setCustomMeta: meta key invalid " + key);
                return false;
            }

            String address = device.getAddress();
            if (VERBOSE) {
                Log.d(TAG, "setCustomMeta: " + address + ", key=" + key);
            }
            if (!mMetadataCache.containsKey(address)) {
                createMetadata(address, false);
            }
            Metadata data = mMetadataCache.get(address);
            byte[] oldValue = data.getCustomizedMeta(key);
            if (oldValue != null && Arrays.equals(oldValue, newValue)) {
                if (VERBOSE) {
                    Log.d(TAG, "setCustomMeta: metadata not changed.");
                }
                return true;
            }
            logManufacturerInfo(device, key, newValue);
            data.setCustomizedMeta(key, newValue);

            updateDatabase(data);
            mAdapterService.metadataChanged(address, key, newValue);
            return true;
        }
    }

    /**
     * Get customized metadata from database with requested key
     */
    @VisibleForTesting
    public byte[] getCustomMeta(BluetoothDevice device, int key) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "getCustomMeta: device is null");
                return null;
            }
            if (!isValidMetaKey(key)) {
                Log.e(TAG, "getCustomMeta: meta key invalid " + key);
                return null;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.e(TAG, "getCustomMeta: device " + address + " is not in cache");
                return null;
            }

            Metadata data = mMetadataCache.get(address);
            return data.getCustomizedMeta(key);
        }
    }

    /**
     * Set the device profile connection policy
     *
     * @param device {@link BluetoothDevice} wish to set
     * @param profile The Bluetooth profile; one of {@link BluetoothProfile#HEADSET},
     * {@link BluetoothProfile#HEADSET_CLIENT}, {@link BluetoothProfile#A2DP},
     * {@link BluetoothProfile#A2DP_SINK}, {@link BluetoothProfile#HID_HOST},
     * {@link BluetoothProfile#PAN}, {@link BluetoothProfile#PBAP},
     * {@link BluetoothProfile#PBAP_CLIENT}, {@link BluetoothProfile#MAP},
     * {@link BluetoothProfile#MAP_CLIENT}, {@link BluetoothProfile#SAP},
     * {@link BluetoothProfile#HEARING_AID}, {@link BluetoothProfile#CSIP_SET_COORDINATOR},
     * {@link BluetoothProfile#LE_AUDIO_BROADCAST_ASSISTANT},{@link BluetoothProfile#VOLUME_CONTROL},
     * @param newConnectionPolicy the connectionPolicy to set; one of
     * {@link BluetoothProfile.CONNECTION_POLICY_UNKNOWN},
     * {@link BluetoothProfile.CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile.CONNECTION_POLICY_ALLOWED}
     */
    @VisibleForTesting
    public boolean setProfileConnectionPolicy(BluetoothDevice device, int profile,
            int newConnectionPolicy) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setProfileConnectionPolicy: device is null");
                return false;
            }

            if (newConnectionPolicy != BluetoothProfile.CONNECTION_POLICY_UNKNOWN
                    && newConnectionPolicy != BluetoothProfile.CONNECTION_POLICY_FORBIDDEN
                    && newConnectionPolicy != BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
                Log.e(TAG, "setProfileConnectionPolicy: invalid connection policy "
                        + newConnectionPolicy);
                return false;
            }

            String address = device.getAddress();
            if (!mMetadataCache.containsKey(address)) {
                if (newConnectionPolicy == BluetoothProfile.CONNECTION_POLICY_UNKNOWN) {
                    return true;
                }
                createMetadata(address, false);
            }

            Metadata data = mMetadataCache.get(address);
            int oldConnectionPolicy = data.getProfileConnectionPolicy(profile);

            if (oldConnectionPolicy == newConnectionPolicy) {
                Log.v(TAG, "setProfileConnectionPolicy connection policy not changed.");
                return true;
            }

            String profileStr = BluetoothProfile.getProfileName(profile);
            data.setProfileConnectionPolicy(profile, newConnectionPolicy);
            updateDatabase(data);
            return true;
        }
    }

    /**
     * Get the device profile connection policy
     *
     * @param device {@link BluetoothDevice} wish to get
     * @param profile The Bluetooth profile; one of {@link BluetoothProfile#HEADSET},
     * {@link BluetoothProfile#HEADSET_CLIENT}, {@link BluetoothProfile#A2DP},
     * {@link BluetoothProfile#A2DP_SINK}, {@link BluetoothProfile#HID_HOST},
     * {@link BluetoothProfile#PAN}, {@link BluetoothProfile#PBAP},
     * {@link BluetoothProfile#PBAP_CLIENT}, {@link BluetoothProfile#MAP},
     * {@link BluetoothProfile#MAP_CLIENT}, {@link BluetoothProfile#SAP},
     * {@link BluetoothProfile#HEARING_AID}, {@link BluetoothProfile#CSIP_SET_COORDINATOR},
     * {@link BluetoothProfile#LE_AUDIO_BROADCAST_ASSISTANT}, {@link BluetoothProfile#VOLUME_CONTROL},
     * @return the profile connection policy of the device; one of
     * {@link BluetoothProfile.CONNECTION_POLICY_UNKNOWN},
     * {@link BluetoothProfile.CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile.CONNECTION_POLICY_ALLOWED}
     */
    @VisibleForTesting
    public int getProfileConnectionPolicy(BluetoothDevice device, int profile) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "getProfileConnectionPolicy: device is null");
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.d(TAG, "getProfileConnectionPolicy: device " + address + " is not in cache");
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }

            Metadata data = mMetadataCache.get(address);
            int connectionPolicy = data.getProfileConnectionPolicy(profile);

            Log.v(TAG, "getProfileConnectionPolicy: " + device.getAnonymizedAddress()
                    + ", profile=" + profile + ", connectionPolicy = " + connectionPolicy);
            return connectionPolicy;
        }
    }

    /**
     * Set the A2DP optional coedc support value
     *
     * @param device {@link BluetoothDevice} wish to set
     * @param newValue the new A2DP optional coedc support value, one of
     * {@link BluetoothA2dp#OPTIONAL_CODECS_SUPPORT_UNKNOWN},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_NOT_SUPPORTED},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_SUPPORTED}
     */
    @VisibleForTesting
    public void setA2dpSupportsOptionalCodecs(BluetoothDevice device, int newValue) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setA2dpOptionalCodec: device is null");
                return;
            }
            if (newValue != BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN
                    && newValue != BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED
                    && newValue != BluetoothA2dp.OPTIONAL_CODECS_SUPPORTED) {
                Log.e(TAG, "setA2dpSupportsOptionalCodecs: invalid value " + newValue);
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            Metadata data = mMetadataCache.get(address);
            int oldValue = data.a2dpSupportsOptionalCodecs;
            if (oldValue == newValue) {
                return;
            }

            data.a2dpSupportsOptionalCodecs = newValue;
            updateDatabase(data);
        }
    }

    /**
     * Get the A2DP optional coedc support value
     *
     * @param device {@link BluetoothDevice} wish to get
     * @return the A2DP optional coedc support value, one of
     * {@link BluetoothA2dp#OPTIONAL_CODECS_SUPPORT_UNKNOWN},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_NOT_SUPPORTED},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_SUPPORTED},
     */
    @VisibleForTesting
    public int getA2dpSupportsOptionalCodecs(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setA2dpOptionalCodec: device is null");
                return BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.e(TAG, "getA2dpOptionalCodec: device " + address + " is not in cache");
                return BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN;
            }

            Metadata data = mMetadataCache.get(address);
            return data.a2dpSupportsOptionalCodecs;
        }
    }

    /**
     * Set the A2DP optional coedc enabled value
     *
     * @param device {@link BluetoothDevice} wish to set
     * @param newValue the new A2DP optional coedc enabled value, one of
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_UNKNOWN},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_DISABLED},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_ENABLED}
     */
    @VisibleForTesting
    public void setA2dpOptionalCodecsEnabled(BluetoothDevice device, int newValue) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setA2dpOptionalCodecEnabled: device is null");
                return;
            }
            if (newValue != BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN
                    && newValue != BluetoothA2dp.OPTIONAL_CODECS_PREF_DISABLED
                    && newValue != BluetoothA2dp.OPTIONAL_CODECS_PREF_ENABLED) {
                Log.e(TAG, "setA2dpOptionalCodecsEnabled: invalid value " + newValue);
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            Metadata data = mMetadataCache.get(address);
            int oldValue = data.a2dpOptionalCodecsEnabled;
            if (oldValue == newValue) {
                return;
            }

            data.a2dpOptionalCodecsEnabled = newValue;
            updateDatabase(data);
        }
    }

    /**
     * Get the A2DP optional coedc enabled value
     *
     * @param device {@link BluetoothDevice} wish to get
     * @return the A2DP optional coedc enabled value, one of
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_UNKNOWN},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_DISABLED},
     * {@link BluetoothA2dp#OPTIONAL_CODECS_PREF_ENABLED}
     */
    @VisibleForTesting
    public int getA2dpOptionalCodecsEnabled(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "getA2dpOptionalCodecEnabled: device is null");
                return BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.e(TAG, "getA2dpOptionalCodecEnabled: device " + address + " is not in cache");
                return BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN;
            }

            Metadata data = mMetadataCache.get(address);
            return data.a2dpOptionalCodecsEnabled;
        }
    }

    /**
     * Updates the time this device was last connected
     *
     * @param device is the remote bluetooth device for which we are setting the connection time
     */
    public void setConnection(BluetoothDevice device, boolean isA2dpDevice) {
        synchronized (mMetadataCache) {
            Log.d(TAG, "setConnection: device=" + device + " and isA2dpDevice=" + isA2dpDevice);
            if (device == null) {
                Log.e(TAG, "setConnection: device is null");
                return;
            }

            if (isA2dpDevice) {
                resetActiveA2dpDevice();
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.d(TAG, "setConnection: Creating new metadata entry for device: " + device);
                createMetadata(address, isA2dpDevice);
                return;
            }
            // Updates last_active_time to the current counter value and increments the counter
            Metadata metadata = mMetadataCache.get(address);
            metadata.last_active_time = MetadataDatabase.sCurrentConnectionNumber++;

            // Only update is_active_a2dp_device if an a2dp device is connected
            if (isA2dpDevice) {
                metadata.is_active_a2dp_device = true;
            }

            Log.d(TAG, "Updating last connected time for device: " + device.getAnonymizedAddress()
                    + " to " + metadata.last_active_time);
            updateDatabase(metadata);
        }
    }

    /**
     * Updates the time this device was last connected with HFP
     *
     * @param device is the remote bluetooth device for which we are setting the connection time
     */
    public void setConnectionForHfp(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            Log.d(TAG, "setConnectionForHfp: device=" + device);
            if (device == null) {
                Log.e(TAG, "setConnectionForHfp: device is null");
                return;
            }

            resetActiveHfpDevice();

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.w(TAG, "setConnectionForHfp: Creating new metadata entry for device: "
                        + device);
                createMetadataHfp(address);
                return;
            }
            // Updates last_active_time to the current counter value and increments the counter
            Metadata metadata = mMetadataCache.get(address);
            metadata.last_active_time = MetadataDatabase.sCurrentConnectionNumber++;

            // Only update is_active_hfp_device if a hfp device is connected
            metadata.is_active_hfp_device = true;

            Log.d(TAG, "Updating last connected time for device: " + device.getAnonymizedAddress()
                    + " to " + metadata.last_active_time);
            updateDatabase(metadata);
        }
    }

    /**
     * Updates the time this device was last connected with LeAudio
     *
     * @param device is the remote bluetooth device for which we are setting the connection time
     */
    public void setConnectionForLeAudio(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            Log.d(TAG, "setConnectionForLeAudio: device=" + device);
            if (device == null) {
                Log.e(TAG, "setConnectionForLeAudio: device is null");
                return;
            }

            resetActiveLeAudioDevice();

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.w(TAG, "setConnectionForLeAudio: Creating new metadata entry for device: "
                        + device);
                createMetadataLeAudio(address);
                return;
            }
            // Updates last_active_time to the current counter value and increments the counter
            Metadata metadata = mMetadataCache.get(address);
            metadata.last_active_time = MetadataDatabase.sCurrentConnectionNumber++;

            // Only update is_active_le_audio_device if a hfp device is connected
            metadata.is_active_le_audio_device = true;

            Log.d(TAG, "Updating last connected time for device: " + device + " to "
                    + metadata.last_active_time);
            updateDatabase(metadata);
        }
    }

    /**
     * Updates the time this device was last connected with A2dpSource
     *
     * @param device is the remote bluetooth device for which we are setting the connection time
     */
    public void setConnectionForA2dpSrc(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            Log.d(TAG, "setConnectionForA2dpSrc: device=" + device);
            if (device == null) {
                Log.e(TAG, "setConnectionForA2dpSrc: device is null");
                return;
            }

            resetConnectedA2dpSrcDevice();

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                Log.w(TAG, "setConnectionForA2dpSrc: Creating new metadata entry for device: "
                        + device);
                createMetadataA2dpSrc(address);
                return;
            }
            // Updates last_active_time to the current counter value and increments the counter
            Metadata metadata = mMetadataCache.get(address);
            metadata.last_active_time = MetadataDatabase.sCurrentConnectionNumber++;

            // Only update is_connected_a2dpsrc_device if a a2dpsrc device is connected
            metadata.is_connected_a2dpsrc_device = true;

            Log.d(TAG, "Updating last connected time for device: " + device.getAnonymizedAddress()
                    + " to " + metadata.last_active_time);
            updateDatabase(metadata);
        }
    }

    /**
     * Sets is_active_device to false if currently true for device
     *
     * @param device is the remote bluetooth device with which we have disconnected a2dp
     */
    public void setDisconnection(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setDisconnection: device is null");
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            // Updates last connected time to either current time if connected or -1 if disconnected
            Metadata metadata = mMetadataCache.get(address);
            if (metadata.is_active_a2dp_device) {
                metadata.is_active_a2dp_device = false;
                Log.d(TAG, "setDisconnection: Updating is_active_device to false for device: "
                        + device.getAnonymizedAddress());
                updateDatabase(metadata);
            }
        }
    }

    /**
     * Sets is_active_hfp_device to false if currently true for device
     *
     * @param device is the remote bluetooth device with which we have disconnected hfp
     */
    public void setDisconnectionForHfp(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setDisconnectionForHfp: device is null");
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            // Updates last connected time to either current time if connected or -1 if disconnected
            Metadata metadata = mMetadataCache.get(address);
            if (metadata.is_active_hfp_device) {
                metadata.is_active_hfp_device = false;
                Log.w(TAG, "setDisconnectionForHfp: Set is_active_hfp_device to false for device: "
                        + device.getAnonymizedAddress());
                updateDatabase(metadata);
            }
        }
    }

    /**
     * Sets is_active_le_audio_device to false if currently true for device
     *
     * @param device is the remote bluetooth device with which we have disconnected le_audio
     */
    public void setDisconnectionForLeAudio(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setDisconnectionForLeAudio: device is null");
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            // Updates last connected time to either current time if connected or -1 if disconnected
            Metadata metadata = mMetadataCache.get(address);
            if (metadata.is_active_le_audio_device) {
                metadata.is_active_le_audio_device = false;
                Log.w(TAG, "setDisconnectionForLeAudio: Set is_active_le_audio_device to false for device: "
                        + device);
                updateDatabase(metadata);
            }
        }
    }

    /**
     * Sets is_connected_a2dpsrc_device to false if currently true for device
     *
     * @param device is the remote bluetooth device with which we have disconnected A2dpSrc
     */
    public void setDisconnectionForA2dpSrc(BluetoothDevice device) {
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "setDisconnectionForA2dpSrc: device is null");
                return;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return;
            }
            // Updates last connected time to either current time if connected or -1 if disconnected
            Metadata metadata = mMetadataCache.get(address);
            if (metadata.is_connected_a2dpsrc_device) {
                metadata.is_connected_a2dpsrc_device = false;
                Log.w(TAG, "setDisconnectionForA2dpSrc: Set is_connected_a2dpsrc_device to false" +
                                     " for device: " + device.getAnonymizedAddress());
                updateDatabase(metadata);
            }
        }
    }

    /**
     * Sets was_previously_connected_to_bc to true/false based on the state
     *
     *
     * @param device is the remote bluetooth device
     * @param state BC profile connection state
     */
    public void setConnectionStateForBc(BluetoothDevice device, int state) {
        synchronized (mMetadataCache) {
        if (device == null) {
            Log.e(TAG, "setConnectionStateForBc: device is null");
            return;
        }

        String address = device.getAddress();

        if (state == BluetoothProfile.STATE_CONNECTED) {
            if (!mMetadataCache.containsKey(address)) {
                Log.w(TAG, "setConnectionStateForBc: Creating new metadata entry for device: "
                        + device);
                createMetadataForBc(address);
                return;
            }
        } else {
            if (!mMetadataCache.containsKey(address)) {
                   return;
            }
        }
        Metadata metadata = mMetadataCache.get(address);
        if (state == BluetoothProfile.STATE_CONNECTED) {
            metadata.was_previously_connected_to_bc = true;
            metadata.device_supports_bc_profile = true;
        } else {
            metadata.was_previously_connected_to_bc = false;
        }
        Log.w(TAG, "setConnectionStateForBc: Set" +
                            metadata.was_previously_connected_to_bc +
                                     " for device: " + device.getAnonymizedAddress());
        updateDatabase(metadata);
         }
    }

    /**
     * check if the BC profile connected earlier
     *
     * @return true if BC profile was connected to this device
     *         false otherwise
     */
    public boolean wasBCConnectedDevice(BluetoothDevice device) {
        boolean ret = false;
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "wasBCConnectedDevice: device is null");
                return false;
            }

            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                   return false;
            }
            Metadata metadata = mMetadataCache.get(address);
            if (metadata != null) {
                ret = metadata.was_previously_connected_to_bc;
            }
        }
        return ret;
    }

    /**
     * check if the BC profile is supported by remote device
     *
     * @return true if BC profile is supported by this device
     *         false otherwise
     */
    public boolean deviceSupportsBCprofile(BluetoothDevice device) {
        boolean ret = false;
        synchronized (mMetadataCache) {
            if (device == null) {
                Log.e(TAG, "deviceSupportsBCprofile: device is null");
                return false;
            }
            String address = device.getAddress();

            if (!mMetadataCache.containsKey(address)) {
                return false;
            }
            Metadata metadata = mMetadataCache.get(address);
            if (metadata != null) {
                Log.v(TAG, "deviceSupportsBCprofile: " +
                                metadata.device_supports_bc_profile);
                ret = metadata.device_supports_bc_profile;
            }
        }
        return ret;
    }

    /**
     * Remove a2dpActiveDevice from the current active device in the connection order table
     */
    private void resetActiveA2dpDevice() {
        synchronized (mMetadataCache) {
            Log.d(TAG, "resetActiveA2dpDevice()");
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_a2dp_device) {
                    Log.d(TAG, "resetActiveA2dpDevice");
                    metadata.is_active_a2dp_device = false;
                    updateDatabase(metadata);
                }
            }
        }
    }

    /**
     * Remove hfpActiveDevice from the current active device in the connection order table
     */
    private void resetActiveHfpDevice() {
        synchronized (mMetadataCache) {
            Log.d(TAG, "resetActiveHfpDevice()");
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_hfp_device) {
                    Log.d(TAG, "resetActiveHfpDevice");
                    metadata.is_active_hfp_device = false;
                    updateDatabase(metadata);
                }
            }
        }
    }

    /**
     * Remove LeAudioActiveDevice from the current active device in the connection order table
     */
    private void resetActiveLeAudioDevice() {
        synchronized (mMetadataCache) {
            Log.d(TAG, "resetActiveLeAudioDevice()");
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_le_audio_device) {
                    Log.d(TAG, "resetActiveLeAudioDevice");
                    metadata.is_active_le_audio_device = false;
                    updateDatabase(metadata);
                }
            }
        }
    }


    /**
     * Remove ConnectedA2dpSrc device from the current connetced device
     * in the connection order table
     */
    private void resetConnectedA2dpSrcDevice() {
        synchronized (mMetadataCache) {
            Log.d(TAG, "resetConnectedA2dpSrcDevice()");
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_connected_a2dpsrc_device) {
                    Log.d(TAG, "resetConnectedA2dpSrcDevice");
                    metadata.is_connected_a2dpsrc_device = false;
                    updateDatabase(metadata);
                }
            }
        }
    }

    /**
     * Gets the most recently connected bluetooth devices in order with most recently connected
     * first and least recently connected last
     *
     * @return a {@link List} of {@link BluetoothDevice} representing connected bluetooth devices
     * in order of most recently connected
     */
    public List<BluetoothDevice> getMostRecentlyConnectedDevices() {
        List<BluetoothDevice> mostRecentlyConnectedDevices = new ArrayList<>();
        synchronized (mMetadataCache) {
            List<Metadata> sortedMetadata = new ArrayList<>(mMetadataCache.values());
            sortedMetadata.sort((o1, o2) -> Long.compare(o2.last_active_time, o1.last_active_time));
            for (Metadata metadata : sortedMetadata) {
                try {
                    mostRecentlyConnectedDevices.add(BluetoothAdapter.getDefaultAdapter()
                            .getRemoteDevice(metadata.getAddress()));
                } catch (IllegalArgumentException ex) {
                    Log.d(TAG, "getBondedDevicesOrdered: Invalid address for "
                            + "device " + metadata.getAddress());
                }
            }
        }
        return mostRecentlyConnectedDevices;
    }

    /**
     * Gets the last active a2dp device
     *
     * @return the most recently active a2dp device or null if the last a2dp device was null
     */
    public BluetoothDevice getMostRecentlyConnectedA2dpDevice() {
        synchronized (mMetadataCache) {
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_a2dp_device) {
                    try {
                        return BluetoothAdapter.getDefaultAdapter().getRemoteDevice(
                                metadata.getAddress());
                    } catch (IllegalArgumentException ex) {
                        Log.d(TAG, "getMostRecentlyConnectedA2dpDevice: Invalid address for "
                                + "device " + metadata.getAddress());
                    }
                }
            }
        }
        return null;
    }

    /**
     * Gets the last active hfp device
     *
     * @return the most recently active hfp device or null if the last hfp device was null
     */
    public BluetoothDevice getMostRecentlyConnectedHfpDevice() {
        synchronized (mMetadataCache) {
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_hfp_device) {
                    try {
                        return BluetoothAdapter.getDefaultAdapter().getRemoteDevice(
                                metadata.getAddress());
                    } catch (IllegalArgumentException ex) {
                        Log.d(TAG, "getMostRecentlyConnectedHfpDevice: Invalid address for "
                                + "device " + metadata.getAddress());
                    }
                }
            }
        }
        return null;
    }

    /**
     * Gets the last active le_audio device
     *
     * @return the most recently active le_audio device or null if the last le_audio device was null
     */
    public BluetoothDevice getMostRecentlyConnectedLeAudioDevice() {
        synchronized (mMetadataCache) {
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_active_le_audio_device) {
                    try {
                        return BluetoothAdapter.getDefaultAdapter().getRemoteDevice(
                                metadata.getAddress());
                    } catch (IllegalArgumentException ex) {
                        Log.d(TAG, "getMostRecentlyConnectedLeAudioDevice: Invalid address for "
                                + "device " + metadata.getAddress());
                    }
                }
            }
        }
        return null;
    }

    /**
     * Gets the last Connected A2dpSource device
     *
     * @return the most recently connected a2dpsource device or null
     * if the last a2dpsource device was null
     */
    public BluetoothDevice getMostRecentlyConnectedA2dpSrcDevice() {
        synchronized (mMetadataCache) {
            for (Map.Entry<String, Metadata> entry : mMetadataCache.entrySet()) {
                Metadata metadata = entry.getValue();
                if (metadata.is_connected_a2dpsrc_device) {
                    try {
                        return BluetoothAdapter.getDefaultAdapter().getRemoteDevice(
                                metadata.getAddress());
                    } catch (IllegalArgumentException ex) {
                        Log.d(TAG, "getMostRecentlyConnectedA2dpSrcDevice: Invalid address for "
                                + "device " + metadata.getAddress());
                    }
                }
            }
        }
        return null;
    }

    /**
     *
     * @param metadataList is the list of metadata
     */
    private void compactLastConnectionTime(List<Metadata> metadataList) {
        Log.d(TAG, "compactLastConnectionTime: Compacting metadata after load");
        MetadataDatabase.sCurrentConnectionNumber = 0;
        // Have to go in reverse order as list is ordered by descending last_active_time
        for (int index = metadataList.size() - 1; index >= 0; index--) {
            Metadata metadata = metadataList.get(index);
            if (metadata.last_active_time != MetadataDatabase.sCurrentConnectionNumber) {
                Log.d(TAG, "compactLastConnectionTime: Setting last_active_item for device: "
                        + metadata.getAddress() + " from " + metadata.last_active_time + " to "
                        + MetadataDatabase.sCurrentConnectionNumber);
                metadata.last_active_time = MetadataDatabase.sCurrentConnectionNumber;
                updateDatabase(metadata);
                MetadataDatabase.sCurrentConnectionNumber++;
            }
        }
    }

    /**
     * Get the {@link Looper} for the handler thread. This is used in testing and helper
     * objects
     *
     * @return {@link Looper} for the handler thread
     */
    @VisibleForTesting
    public Looper getHandlerLooper() {
        if (mHandlerThread == null) {
            return null;
        }
        return mHandlerThread.getLooper();
    }

    /**
     * Start and initialize the DatabaseManager
     *
     * @param database the Bluetooth storage {@link MetadataDatabase}
     */
    public void start(MetadataDatabase database) {
        if (DBG) {
            Log.d(TAG, "start()");
        }

        if (mAdapterService == null) {
            Log.e(TAG, "stat failed, mAdapterService is null.");
            return;
        }

        if (database == null) {
            Log.e(TAG, "stat failed, database is null.");
            return;
        }

        mDatabase = database;

        mHandlerThread = new HandlerThread("BluetoothDatabaseManager");
        mHandlerThread.start();
        mHandler = new DatabaseHandler(mHandlerThread.getLooper());

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        mAdapterService.registerReceiver(mReceiver, filter);

        loadDatabase();
    }

    String getDatabaseAbsolutePath() {
        //TODO backup database when Bluetooth turn off and FOTA?
        return mAdapterService.getDatabasePath(MetadataDatabase.DATABASE_NAME)
                .getAbsolutePath();
    }

    /**
     * Clear all persistence data in database
     */
    public void factoryReset() {
        Log.w(TAG, "factoryReset");
        Message message = mHandler.obtainMessage(MSG_CLEAR_DATABASE);
        mHandler.sendMessage(message);
    }

    /**
     * Close and de-init the DatabaseManager
     */
    public void cleanup() {
        removeUnusedMetadata();
        mAdapterService.unregisterReceiver(mReceiver);
        if (mHandlerThread != null) {
            mHandlerThread.quit();
            mHandlerThread = null;
        }
        mMetadataCache.clear();
    }

    void createMetadata(String address, boolean isActiveA2dpDevice) {
        Metadata data = new Metadata(address);
        data.is_active_a2dp_device = isActiveA2dpDevice;
        mMetadataCache.put(address, data);
        updateDatabase(data);
    }

    void createMetadataHfp(String address) {

        // TODO: cross check the logic
        Metadata data;
        if (mMetadataCache.containsKey(address))
           data = mMetadataCache.get(address);
        else
           data = new Metadata(address);
        data.is_active_hfp_device = true;
        mMetadataCache.put(address, data);
        updateDatabase(data);
    }

    void createMetadataLeAudio(String address) {
        Metadata data;
        if (mMetadataCache.containsKey(address))
           data = mMetadataCache.get(address);
        else
           data = new Metadata(address);
        data.is_active_le_audio_device = true;
        mMetadataCache.put(address, data);
        updateDatabase(data);
    }

    void createMetadataA2dpSrc(String address) {
        // TODO: cross check the logic
        Metadata data;
        if (mMetadataCache.containsKey(address))
           data = mMetadataCache.get(address);
        else
           data = new Metadata(address);
        data.is_connected_a2dpsrc_device = true;
        mMetadataCache.put(address, data);
        updateDatabase(data);
    }

    void createMetadataForBc(String address) {
        Metadata data;
        if (mMetadataCache.containsKey(address))
           data = mMetadataCache.get(address);
        else
           data = new Metadata(address);
        data.was_previously_connected_to_bc = true;
        data.device_supports_bc_profile = true;
        mMetadataCache.put(address, data);
        updateDatabase(data);
    }

    @VisibleForTesting
    void removeUnusedMetadata() {
        BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        synchronized (mMetadataCache) {
            mMetadataCache.forEach((address, metadata) -> {
                if (!address.equals(LOCAL_STORAGE)
                        && !Arrays.asList(bondedDevices).stream().anyMatch(device ->
                        address.equals(device.getAddress()))) {
                    List<Integer> list = metadata.getChangedCustomizedMeta();
                    for (int key : list) {
                        mAdapterService.metadataChanged(address, key, null);
                    }
                    Log.i(TAG, "remove unpaired device from database " + address);
                    deleteDatabase(mMetadataCache.get(address));
                }
            });
        }
    }

    void cacheMetadata(List<Metadata> list) {
        synchronized (mMetadataCache) {
            Log.i(TAG, "cacheMetadata");
            // Unlock the main thread.
            mSemaphore.release();

            if (!isMigrated(list)) {
                // Wait for data migrate from Settings Global
                mMigratedFromSettingsGlobal = false;
                return;
            }
            mMigratedFromSettingsGlobal = true;
            for (Metadata data : list) {
                String address = data.getAddress();
                if (VERBOSE) {
                    Log.v(TAG, "cacheMetadata: found device " + address);
                }
                mMetadataCache.put(address, data);
            }
            if (VERBOSE) {
                Log.v(TAG, "cacheMetadata: Database is ready");
            }
        }
    }

    boolean isMigrated(List<Metadata> list) {
        for (Metadata data : list) {
            String address = data.getAddress();
            if (address.equals(LOCAL_STORAGE) && data.migrated) {
                return true;
            }
        }
        return false;
    }

    void migrateSettingsGlobal() {
        Log.i(TAG, "migrateSettingGlobal");

        BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        ContentResolver contentResolver = mAdapterService.getContentResolver();

        for (BluetoothDevice device : bondedDevices) {
            int a2dpConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyA2dpSinkPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int a2dpSinkConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyA2dpSrcPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int hearingaidConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyHearingAidPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int headsetConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyHeadsetPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int headsetClientConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyHeadsetPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int hidHostConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyHidHostPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int mapConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyMapPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int mapClientConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyMapClientPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int panConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyPanPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int pbapConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyPbapClientPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int pbapClientConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacyPbapClientPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int sapConnectionPolicy = Settings.Global.getInt(contentResolver,
                    getLegacySapPriorityKey(device.getAddress()),
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            int a2dpSupportsOptionalCodec = Settings.Global.getInt(contentResolver,
                    getLegacyA2dpSupportsOptionalCodecsKey(device.getAddress()),
                    BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
            int a2dpOptionalCodecEnabled = Settings.Global.getInt(contentResolver,
                    getLegacyA2dpOptionalCodecsEnabledKey(device.getAddress()),
                    BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);

            String address = device.getAddress();
            Metadata data = new Metadata(address);
            data.setProfileConnectionPolicy(BluetoothProfile.A2DP, a2dpConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.A2DP_SINK, a2dpSinkConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.HEADSET, headsetConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.HEADSET_CLIENT,
                    headsetClientConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.HID_HOST, hidHostConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.PAN, panConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.PBAP, pbapConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.PBAP_CLIENT,
                    pbapClientConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.MAP, mapConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.MAP_CLIENT, mapClientConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.SAP, sapConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.HEARING_AID,
                    hearingaidConnectionPolicy);
            data.setProfileConnectionPolicy(BluetoothProfile.LE_AUDIO,
                    BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
            data.a2dpSupportsOptionalCodecs = a2dpSupportsOptionalCodec;
            data.a2dpOptionalCodecsEnabled = a2dpOptionalCodecEnabled;
            mMetadataCache.put(address, data);
            updateDatabase(data);
        }

        // Mark database migrated from Settings Global
        Metadata localData = new Metadata(LOCAL_STORAGE);
        localData.migrated = true;
        mMetadataCache.put(LOCAL_STORAGE, localData);
        updateDatabase(localData);

        // Reload database after migration is completed
        loadDatabase();

    }

    /**
     * Get the key that retrieves a bluetooth headset's priority.
     */
    private static String getLegacyHeadsetPriorityKey(String address) {
        return LEGACY_HEADSET_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth a2dp sink's priority.
     */
    private static String getLegacyA2dpSinkPriorityKey(String address) {
        return LEGACY_A2DP_SINK_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth a2dp src's priority.
     */
    private static String getLegacyA2dpSrcPriorityKey(String address) {
        return LEGACY_A2DP_SRC_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth a2dp device's ability to support optional codecs.
     */
    private static String getLegacyA2dpSupportsOptionalCodecsKey(String address) {
        return LEGACY_A2DP_SUPPORTS_OPTIONAL_CODECS_PREFIX
                + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves whether a bluetooth a2dp device should have optional codecs
     * enabled.
     */
    private static String getLegacyA2dpOptionalCodecsEnabledKey(String address) {
        return LEGACY_A2DP_OPTIONAL_CODECS_ENABLED_PREFIX
                + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth Input Device's priority.
     */
    private static String getLegacyHidHostPriorityKey(String address) {
        return LEGACY_INPUT_DEVICE_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth pan client priority.
     */
    private static String getLegacyPanPriorityKey(String address) {
        return LEGACY_PAN_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth hearing aid priority.
     */
    private static String getLegacyHearingAidPriorityKey(String address) {
        return LEGACY_HEARING_AID_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth map priority.
     */
    private static String getLegacyMapPriorityKey(String address) {
        return LEGACY_MAP_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth map client priority.
     */
    private static String getLegacyMapClientPriorityKey(String address) {
        return LEGACY_MAP_CLIENT_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth pbap client priority.
     */
    private static String getLegacyPbapClientPriorityKey(String address) {
        return LEGACY_PBAP_CLIENT_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    /**
     * Get the key that retrieves a bluetooth sap priority.
     */
    private static String getLegacySapPriorityKey(String address) {
        return LEGACY_SAP_PRIORITY_PREFIX + address.toUpperCase(Locale.ROOT);
    }

    private void loadDatabase() {
        if (DBG) {
            Log.d(TAG, "Load Database");
        }
        Message message = mHandler.obtainMessage(MSG_LOAD_DATABASE);
        mHandler.sendMessage(message);
        try {
            // Lock the thread until handler thread finish loading database.
            mSemaphore.tryAcquire(LOAD_DATABASE_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "loadDatabase: semaphore acquire failed");
        }
    }

    private void updateDatabase(Metadata data) {
        if (data.getAddress() == null) {
            Log.e(TAG, "updateDatabase: address is null");
            return;
        }
        if (DBG) {
            Log.d(TAG, "updateDatabase " + data.getAddress());
        }
        Message message = mHandler.obtainMessage(MSG_UPDATE_DATABASE);
        message.obj = data;
        mHandler.sendMessage(message);
    }

    @VisibleForTesting
    void deleteDatabase(Metadata data) {
        if (data.getAddress() == null) {
            Log.e(TAG, "deleteDatabase: address is null");
            return;
        }
        Log.d(TAG, "deleteDatabase: " + data.getAddress());
        Message message = mHandler.obtainMessage(MSG_DELETE_DATABASE);
        message.obj = data.getAddress();
        mHandler.sendMessage(message);
    }

    private void logManufacturerInfo(BluetoothDevice device, int key, byte[] bytesValue) {
        String callingApp = mAdapterService.getPackageManager().getNameForUid(
                Binder.getCallingUid());
        String manufacturerName = "";
        String modelName = "";
        String hardwareVersion = "";
        String softwareVersion = "";
        String value = Utils.byteArrayToUtf8String(bytesValue);
        switch (key) {
            case BluetoothDevice.METADATA_MANUFACTURER_NAME:
                manufacturerName = value;
                break;
            case BluetoothDevice.METADATA_MODEL_NAME:
                modelName = value;
                break;
            case BluetoothDevice.METADATA_HARDWARE_VERSION:
                hardwareVersion = value;
                break;
            case BluetoothDevice.METADATA_SOFTWARE_VERSION:
                softwareVersion = value;
                break;
            default:
                // Do not log anything if metadata doesn't fall into above categories
                return;
        }
        /*BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_DEVICE_INFO_REPORTED,
                mAdapterService.obfuscateAddress(device),
                BluetoothProtoEnums.DEVICE_INFO_EXTERNAL, callingApp, manufacturerName, modelName,
                hardwareVersion, softwareVersion, 0);*/
    }
}
