/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.bluetooth.a2dp;

import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.os.SystemProperties;
import android.util.Log;
import com.android.bluetooth.R;
import com.android.bluetooth.btservice.AdapterService;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;
/*
 * A2DP Codec Configuration setup.
 */
class A2dpCodecConfig {
    private static final boolean DBG = true;
    private static final String TAG = "A2dpCodecConfig";

    private Context mContext;
    private A2dpNativeInterface mA2dpNativeInterface;

    private BluetoothCodecConfig[] mCodecConfigPriorities;
    private int mA2dpSourceCodecPrioritySbc = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityAac = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityAptx = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityAptxHd = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityAptxAdaptive = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityLdac = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityAptxTwsp = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int mA2dpSourceCodecPriorityLc3 = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
    private int assigned_codec_length = 0;
    A2dpCodecConfig(Context context, A2dpNativeInterface a2dpNativeInterface) {
        mContext = context;
        mA2dpNativeInterface = a2dpNativeInterface;
        mCodecConfigPriorities = assignCodecConfigPriorities();
    }

    BluetoothCodecConfig[] codecConfigPriorities() {
        return mCodecConfigPriorities;
    }

    void setCodecConfigPreference(BluetoothDevice device,
                                  BluetoothCodecStatus codecStatus,
                                  BluetoothCodecConfig newCodecConfig) {
        Objects.requireNonNull(codecStatus);

        // Check whether the codecConfig is selectable for this Bluetooth device.
        List<BluetoothCodecConfig> selectableCodecs = codecStatus.getCodecsSelectableCapabilities();
        if (!selectableCodecs.stream().anyMatch(codec ->
                codec.isMandatoryCodec())) {
            // Do not set codec preference to native if the selectableCodecs not contain mandatory
            // codec. The reason could be remote codec negotiation is not completed yet.
            Log.w(TAG, "setCodecConfigPreference: must have mandatory codec before changing.");
            return;
        }
        if (!codecStatus.isCodecConfigSelectable(newCodecConfig)) {
            Log.w(TAG, "setCodecConfigPreference: invalid codec "
                    + Objects.toString(newCodecConfig));
            return;
        }

        // Check whether the codecConfig would change current codec config.
        int prioritizedCodecType = getPrioitizedCodecType(newCodecConfig, selectableCodecs);
        BluetoothCodecConfig currentCodecConfig = codecStatus.getCodecConfig();
        if (prioritizedCodecType == currentCodecConfig.getCodecType()
                && (prioritizedCodecType != newCodecConfig.getCodecType()
                || (currentCodecConfig.similarCodecFeedingParameters(newCodecConfig)
                && currentCodecConfig.sameCodecSpecificParameters(newCodecConfig)))) {
            // Same codec with same parameters, no need to send this request to native.
            Log.w(TAG, "setCodecConfigPreference: codec not changed.");
            return;
        }

        BluetoothCodecConfig[] codecConfigArray = new BluetoothCodecConfig[1];
        codecConfigArray[0] = newCodecConfig;
        mA2dpNativeInterface.setCodecConfigPreference(device, codecConfigArray);
    }

    boolean enableOptionalCodecs(BluetoothDevice device, BluetoothCodecConfig currentCodecConfig) {
        if (currentCodecConfig != null && !currentCodecConfig.isMandatoryCodec()) {
            Log.i(TAG, "enableOptionalCodecs: already using optional codec "
                + BluetoothCodecConfig.getCodecName( currentCodecConfig.getCodecType()));
            return true;
        }

        BluetoothCodecConfig[] codecConfigArray = assignCodecConfigPriorities();
        if (codecConfigArray == null) {
            return true;
        }

        // Set the mandatory codec's priority to default, and remove the rest
        for (int i = 0; i < assigned_codec_length; i++) {
            BluetoothCodecConfig codecConfig = codecConfigArray[i];
            if (!codecConfig.isMandatoryCodec()) {
                codecConfigArray[i] = null;
            }
        }

        return mA2dpNativeInterface.setCodecConfigPreference(device, codecConfigArray);
    }

    boolean disableOptionalCodecs(BluetoothDevice device, BluetoothCodecConfig currentCodecConfig) {
        if (currentCodecConfig != null && currentCodecConfig.isMandatoryCodec()) {
            Log.i(TAG, "disableOptionalCodecs: already using mandatory codec.");
            return true;
        }

        BluetoothCodecConfig[] codecConfigArray = assignCodecConfigPriorities();
        if (codecConfigArray == null) {
            return true;
        }
        // Set the mandatory codec's priority to highest, and remove the rest
        for (int i = 0; i < assigned_codec_length; i++) {
            BluetoothCodecConfig codecConfig = codecConfigArray[i];
            if (codecConfig.isMandatoryCodec()) {
                codecConfig.setCodecPriority(BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST);
            } else {
                codecConfigArray[i] = null;
            }
        }
        return mA2dpNativeInterface.setCodecConfigPreference(device, codecConfigArray);
    }

    // Get the codec type of the highest priority of selectableCodecs and codecConfig.
    private int getPrioitizedCodecType(BluetoothCodecConfig codecConfig,
            List<BluetoothCodecConfig> selectableCodecs) {
        BluetoothCodecConfig prioritizedCodecConfig = codecConfig;
        for (BluetoothCodecConfig config : selectableCodecs) {
            if (prioritizedCodecConfig == null) {
                prioritizedCodecConfig = config;
            }
            if (config.getCodecPriority() > prioritizedCodecConfig.getCodecPriority()) {
                prioritizedCodecConfig = config;
            }
        }
        return prioritizedCodecConfig.getCodecType();
    }

    // Assign the A2DP Source codec config priorities
    private BluetoothCodecConfig[] assignCodecConfigPriorities() {
        Resources resources = mContext.getResources();
        if (resources == null) {
            return null;
        }

        int value;
        AdapterService mAdapterService = AdapterService.getAdapterService();
        String a2dp_offload_cap = mAdapterService.getA2apOffloadCapability();
        try {
            value = resources.getInteger(R.integer.a2dp_source_codec_priority_sbc);
        } catch (NotFoundException e) {
            value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
        }
        if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
            mA2dpSourceCodecPrioritySbc = value;
            if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                !a2dp_offload_cap.contains("sbc")) {
                mA2dpSourceCodecPrioritySbc = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
            }
        }

        try {
            value = resources.getInteger(R.integer.a2dp_source_codec_priority_aac);
        } catch (NotFoundException e) {
            value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
        }
        if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
            mA2dpSourceCodecPriorityAac = value;
            if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                !a2dp_offload_cap.contains("aac")) {
                mA2dpSourceCodecPriorityAac = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
            }
        }

        try {
            value = resources.getInteger(R.integer.a2dp_source_codec_priority_aptx);
        } catch (NotFoundException e) {
            value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
        }
        if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
            mA2dpSourceCodecPriorityAptx = value;
            if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                !(a2dp_offload_cap.contains("aptx-") || (a2dp_offload_cap.endsWith("aptx")))) {
                mA2dpSourceCodecPriorityAptx = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
            }
        }
        if(mAdapterService.isSplitA2DPSourceAPTXADAPTIVE()) {
            try {
                value = resources.getInteger(R.integer.a2dp_source_codec_priority_aptx_adaptive);
            } catch (NotFoundException e) {
                value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
            }
            if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                    < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
                if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty()) {
                    if(a2dp_offload_cap.contains("aptxadaptiver2")) {
                        int aptxaa_r2_priority;
                        try {
                            aptxaa_r2_priority = resources.getInteger(R.integer.a2dp_source_codec_priority_max);
                        } catch (NotFoundException e) {
                            aptxaa_r2_priority = value;
                        }
                        value = aptxaa_r2_priority;
                    } else if(!a2dp_offload_cap.contains("aptxadaptive")) {
                        value = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
                        Log.w(TAG, "Disable Aptx Adaptive. Entry not present in offload property");
                    }
                }
                mA2dpSourceCodecPriorityAptxAdaptive = value;
                Log.i(TAG, "Aptx Adaptive priority: " + mA2dpSourceCodecPriorityAptxAdaptive);
            }
        } else {
            mA2dpSourceCodecPriorityAptxAdaptive = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
        }

        if(mAdapterService.isSplitA2DPSourceAPTXHD()) {
            try {
                value = resources.getInteger(R.integer.a2dp_source_codec_priority_aptx_hd);
            } catch (NotFoundException e) {
                value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
            }
            if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                    < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
                mA2dpSourceCodecPriorityAptxHd = value;
                if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                    !a2dp_offload_cap.contains("aptxhd")) {
                    mA2dpSourceCodecPriorityAptxHd = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
                }
            }
        } else {
            mA2dpSourceCodecPriorityAptxHd = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
        }


        if(mAdapterService.isSplitA2DPSourceLDAC()) {
            try {
                value = resources.getInteger(R.integer.a2dp_source_codec_priority_ldac);
            } catch (NotFoundException e) {
                value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
            }
            if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                    < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
                mA2dpSourceCodecPriorityLdac = value;
                if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                    !a2dp_offload_cap.contains("ldac")) {
                    mA2dpSourceCodecPriorityLdac = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
                }
            }
        } else {
            mA2dpSourceCodecPriorityLdac = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
        }
        if (mAdapterService.isVendorIntfEnabled()) {
            try {
                value = resources.getInteger(R.integer.a2dp_source_codec_priority_aptx_tws);
            } catch (NotFoundException e) {
                value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
            }
            if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                    < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
                mA2dpSourceCodecPriorityAptxTwsp = value;
                if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                    !a2dp_offload_cap.contains("aptxtws")) {
                    mA2dpSourceCodecPriorityAptxTwsp = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
                }
            }
        } else {
            mA2dpSourceCodecPriorityAptxTwsp = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
        }

        try {
            value = resources.getInteger(R.integer.a2dp_source_codec_priority_lc3);
        } catch (NotFoundException e) {
            value = BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT;
        }
        if ((value >= BluetoothCodecConfig.CODEC_PRIORITY_DISABLED) && (value
                < BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST)) {
            mA2dpSourceCodecPriorityLc3 = value;
            if (a2dp_offload_cap != null && !a2dp_offload_cap.isEmpty() &&
                !a2dp_offload_cap.contains("lc3")) {
                mA2dpSourceCodecPriorityLc3 = BluetoothCodecConfig.CODEC_PRIORITY_DISABLED;
            }
        }

        BluetoothCodecConfig codecConfig;
        BluetoothCodecConfig[] codecConfigArray;
        int codecCount = 0;
        codecConfigArray =
                new BluetoothCodecConfig[BluetoothCodecConfig.SOURCE_QVA_CODEC_TYPE_MAX];

        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                mA2dpSourceCodecPrioritySbc, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                mA2dpSourceCodecPriorityAac, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX,
                mA2dpSourceCodecPriorityAptx, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD,
                mA2dpSourceCodecPriorityAptxHd, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                mA2dpSourceCodecPriorityLdac, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        if (mAdapterService.isVendorIntfEnabled()) {
            codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_TWSP,
                    mA2dpSourceCodecPriorityAptxTwsp, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                    BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                    .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                    0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
            codecConfigArray[codecCount++] = codecConfig;
        }

        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_ADAPTIVE,
                mA2dpSourceCodecPriorityAptxAdaptive, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;

        codecConfig = new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_LC3,
                mA2dpSourceCodecPriorityLc3, BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE, BluetoothCodecConfig
                .CHANNEL_MODE_NONE, 0 /* codecSpecific1 */,
                0 /* codecSpecific2 */, 0 /* codecSpecific3 */, 0 /* codecSpecific4 */);
        codecConfigArray[codecCount++] = codecConfig;
        assigned_codec_length = codecCount;
        return codecConfigArray;
    }
}
