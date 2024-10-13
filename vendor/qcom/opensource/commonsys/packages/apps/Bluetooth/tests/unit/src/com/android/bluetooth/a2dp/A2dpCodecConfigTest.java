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

package com.android.bluetooth.a2dp;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.res.Resources;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class A2dpCodecConfigTest {
    private Context mTargetContext;
    private BluetoothDevice mTestDevice;
    private A2dpCodecConfig mA2dpCodecConfig;

    @Mock private Context mMockContext;
    @Mock private Resources mMockResources;
    @Mock private A2dpNativeInterface mA2dpNativeInterface;

    private static final int[] sOptionalCodecTypes = new int[] {
            BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
            BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX,
            BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD,
            BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC
    };

    // Not use the default value to make sure it reads from config
    private static final int SBC_PRIORITY_DEFAULT = 1001;
    private static final int AAC_PRIORITY_DEFAULT = 3001;
    private static final int APTX_PRIORITY_DEFAULT = 5001;
    private static final int APTX_HD_PRIORITY_DEFAULT = 7001;
    private static final int LDAC_PRIORITY_DEFAULT = 9001;
    private static final int PRIORITY_HIGH = 1000000;

    private static final BluetoothCodecConfig[] sCodecCapabilities = new BluetoothCodecConfig[] {
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                                     SBC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_MONO
                                     | BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                                     AAC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX,
                                     APTX_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100
                                     | BluetoothCodecConfig.SAMPLE_RATE_48000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD,
                                     APTX_HD_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100
                                     | BluetoothCodecConfig.SAMPLE_RATE_48000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_24,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                                     LDAC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100
                                     | BluetoothCodecConfig.SAMPLE_RATE_48000
                                     | BluetoothCodecConfig.SAMPLE_RATE_88200
                                     | BluetoothCodecConfig.SAMPLE_RATE_96000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16
                                     | BluetoothCodecConfig.BITS_PER_SAMPLE_24
                                     | BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0)        // Codec-specific fields
    };

    private static final BluetoothCodecConfig[] sDefaultCodecConfigs = new BluetoothCodecConfig[] {
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                                     SBC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                                     AAC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_44100,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX,
                                     APTX_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_48000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD,
                                     APTX_HD_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_48000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_24,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0),       // Codec-specific fields
            new BluetoothCodecConfig(BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                                     LDAC_PRIORITY_DEFAULT,
                                     BluetoothCodecConfig.SAMPLE_RATE_96000,
                                     BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                                     BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                                     0, 0, 0, 0)        // Codec-specific fields
    };

    @Before
    public void setUp() throws Exception {
        // Set up mocks and test assets
        MockitoAnnotations.initMocks(this);
        mTargetContext = InstrumentationRegistry.getTargetContext();

        when(mMockContext.getResources()).thenReturn(mMockResources);
        when(mMockResources.getInteger(R.integer.a2dp_source_codec_priority_sbc))
                .thenReturn(SBC_PRIORITY_DEFAULT);
        when(mMockResources.getInteger(R.integer.a2dp_source_codec_priority_aac))
                .thenReturn(AAC_PRIORITY_DEFAULT);
        when(mMockResources.getInteger(R.integer.a2dp_source_codec_priority_aptx))
                .thenReturn(APTX_PRIORITY_DEFAULT);
        when(mMockResources.getInteger(R.integer.a2dp_source_codec_priority_aptx_hd))
                .thenReturn(APTX_HD_PRIORITY_DEFAULT);
        when(mMockResources.getInteger(R.integer.a2dp_source_codec_priority_ldac))
                .thenReturn(LDAC_PRIORITY_DEFAULT);

        mA2dpCodecConfig = new A2dpCodecConfig(mMockContext, mA2dpNativeInterface);
        mTestDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice("00:01:02:03:04:05");

        doReturn(true).when(mA2dpNativeInterface).setCodecConfigPreference(
                any(BluetoothDevice.class),
                any(BluetoothCodecConfig[].class));
    }

    @After
    public void tearDown() throws Exception {
    }

    @Test
    public void testAssignCodecConfigPriorities() {
        BluetoothCodecConfig[] codecConfigs = mA2dpCodecConfig.codecConfigPriorities();
        for (BluetoothCodecConfig config : codecConfigs) {
            switch(config.getCodecType()) {
                case BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC:
                    Assert.assertEquals(config.getCodecPriority(), SBC_PRIORITY_DEFAULT);
                    break;
                case BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC:
                    Assert.assertEquals(config.getCodecPriority(), AAC_PRIORITY_DEFAULT);
                    break;
                case BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX:
                    Assert.assertEquals(config.getCodecPriority(), APTX_PRIORITY_DEFAULT);
                    break;
                case BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD:
                    Assert.assertEquals(config.getCodecPriority(), APTX_HD_PRIORITY_DEFAULT);
                    break;
                case BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC:
                    Assert.assertEquals(config.getCodecPriority(), LDAC_PRIORITY_DEFAULT);
                    break;
            }
        }
    }

    /**
     * Test that we can fallback to default codec by lower the codec priority we changed before.
     */
    @Test
    public void testSetCodecPreference_priorityHighToDefault() {
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, SBC_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, AAC_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX, APTX_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD, APTX_HD_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, PRIORITY_HIGH,
                false);
    }

    /**
     * Test that we can change the default codec to another by raising the codec priority.
     * LDAC is the default highest codec, so no need to test others.
     */
    @Test
    public void testSetCodecPreference_priorityDefaultToRaiseHigh() {
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, LDAC_PRIORITY_DEFAULT,
                false);
    }

    @Test
    public void testSetCodecPreference_prioritySbcHighToOthersHigh() {
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                false);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                true);
    }

    @Test
    public void testSetCodecPreference_priorityAacHighToOthersHigh() {
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                false);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_APTX_HD, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                true);
        testCodecPriorityChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC, PRIORITY_HIGH,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC, PRIORITY_HIGH,
                true);
    }

    @Test
    public void testSetCodecPreference_parametersChangedInSameCodec() {
        // The SBC default / preferred config is Stereo
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        // SBC Mono is mandatory
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_MONO,
                true);

        // The LDAC default / preferred config within mDefaultCodecConfigs
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);

        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);

        // None for system default (Developer options)
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                false);

        int unsupportedParameter = 0xc0;
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                unsupportedParameter,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                unsupportedParameter,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                unsupportedParameter,
                false);

        int multipleSupportedParameters = 0x03;
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                multipleSupportedParameters,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                multipleSupportedParameters,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                multipleSupportedParameters,
                false);
    }

    @Test
    public void testSetCodecPreference_parametersChangedToAnotherCodec() {
        // different sample rate (44.1 kHz -> 96 kHz)
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_96000,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        // different bits per channel (16 bits -> 32 bits)
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_32,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        // change all PCM parameters
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_MONO,
                true);

        // None for system default (Developer options)
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                true);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                true);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                true);

        int unsupportedParameter = 0xc0;
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                unsupportedParameter,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                unsupportedParameter,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                unsupportedParameter,
                false);

        int multipleSupportedParameters = 0x03;
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                multipleSupportedParameters,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                multipleSupportedParameters,
                BluetoothCodecConfig.CHANNEL_MODE_STEREO,
                false);
        testCodecParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_AAC,
                BluetoothCodecConfig.SAMPLE_RATE_44100,
                BluetoothCodecConfig.BITS_PER_SAMPLE_16,
                multipleSupportedParameters,
                false);
    }

    @Test
    public void testSetCodecPreference_ldacCodecSpecificFieldChanged() {
        int ldacAudioQualityHigh = 1000;
        int ldacAudioQualityABR = 1003;
        int sbcCodecSpecificParameter = 0;
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                false);
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityHigh,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                true);
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                sbcCodecSpecificParameter,
                true);

        // Only LDAC will check the codec specific1 field, but not SBC
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                sbcCodecSpecificParameter,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                true);
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                ldacAudioQualityABR,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC,
                ldacAudioQualityABR,
                true);
        testCodecSpecificParametersChangeHelper(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                ldacAudioQualityHigh,
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                sbcCodecSpecificParameter,
                false);
    }

    @Test
    public void testDisableOptionalCodecs() {
        BluetoothCodecConfig[] codecConfigsArray =
                new BluetoothCodecConfig[BluetoothCodecConfig.SOURCE_CODEC_TYPE_MAX];
        codecConfigsArray[0] = new BluetoothCodecConfig(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                0, 0, 0, 0);       // Codec-specific fields

        // shouldn't invoke to native when current codec is SBC
        mA2dpCodecConfig.disableOptionalCodecs(
                mTestDevice,
                getDefaultCodecConfigByType(
                        BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                        BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT));
        verify(mA2dpNativeInterface, times(0)).setCodecConfigPreference(mTestDevice,
                                                                        codecConfigsArray);

        // should invoke to native when current codec is an optional codec
        int invokedCounter = 0;
        for (int codecType : sOptionalCodecTypes) {
            mA2dpCodecConfig.disableOptionalCodecs(
                    mTestDevice,
                    getDefaultCodecConfigByType(codecType,
                                                BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT));
            verify(mA2dpNativeInterface, times(++invokedCounter))
                    .setCodecConfigPreference(mTestDevice, codecConfigsArray);
        }
    }

    @Test
    public void testEnableOptionalCodecs() {
        BluetoothCodecConfig[] codecConfigsArray =
                new BluetoothCodecConfig[BluetoothCodecConfig.SOURCE_CODEC_TYPE_MAX];
        codecConfigsArray[0] = new BluetoothCodecConfig(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                SBC_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                0, 0, 0, 0);       // Codec-specific fields

        // should invoke to native when current codec is SBC
        mA2dpCodecConfig.enableOptionalCodecs(
                mTestDevice,
                getDefaultCodecConfigByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC,
                                            BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT));
        verify(mA2dpNativeInterface, times(1))
                .setCodecConfigPreference(mTestDevice, codecConfigsArray);

        // shouldn't invoke to native when current codec is already an optional
        for (int codecType : sOptionalCodecTypes) {
            mA2dpCodecConfig.enableOptionalCodecs(
                    mTestDevice,
                    getDefaultCodecConfigByType(codecType,
                                                BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT));
            verify(mA2dpNativeInterface, times(1))
                    .setCodecConfigPreference(mTestDevice, codecConfigsArray);
        }
    }

    private BluetoothCodecConfig getDefaultCodecConfigByType(int codecType, int codecPriority) {
        for (BluetoothCodecConfig codecConfig : sDefaultCodecConfigs) {
            if (codecConfig.getCodecType() != codecType) {
                continue;
            }
            return new BluetoothCodecConfig(
                    codecConfig.getCodecType(),
                    (codecPriority != BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT
                     ? codecPriority : codecConfig.getCodecPriority()),
                    codecConfig.getSampleRate(), codecConfig.getBitsPerSample(),
                    codecConfig.getChannelMode(), codecConfig.getCodecSpecific1(),
                    codecConfig.getCodecSpecific2(), codecConfig.getCodecSpecific3(),
                    codecConfig.getCodecSpecific4());
        }
        Assert.fail("getDefaultCodecConfigByType: No such codecType=" + codecType
                + " in sDefaultCodecConfigs");
        return null;
    }

    private BluetoothCodecConfig getCodecCapabilitiesByType(int codecType) {
        for (BluetoothCodecConfig codecCapabilities : sCodecCapabilities) {
            if (codecCapabilities.getCodecType() != codecType) {
                continue;
            }
            return new BluetoothCodecConfig(
                    codecCapabilities.getCodecType(), codecCapabilities.getCodecPriority(),
                    codecCapabilities.getSampleRate(), codecCapabilities.getBitsPerSample(),
                    codecCapabilities.getChannelMode(), codecCapabilities.getCodecSpecific1(),
                    codecCapabilities.getCodecSpecific2(), codecCapabilities.getCodecSpecific3(),
                    codecCapabilities.getCodecSpecific4());
        }
        Assert.fail("getCodecCapabilitiesByType: No such codecType=" + codecType
                + " in sCodecCapabilities");
        return null;
    }

    private void testCodecParametersChangeHelper(int newCodecType, int oldCodecType,
            int sampleRate, int bitsPerSample, int channelMode, boolean invokeNative) {
        BluetoothCodecConfig oldCodecConfig =
                getDefaultCodecConfigByType(oldCodecType, PRIORITY_HIGH);
        BluetoothCodecConfig[] newCodecConfigsArray = new BluetoothCodecConfig[] {
                new BluetoothCodecConfig(newCodecType,
                                         PRIORITY_HIGH,
                                         sampleRate, bitsPerSample, channelMode,
                                         0, 0, 0, 0)       // Codec-specific fields
        };

        // Test cases: 1. no mandatory; 2. mandatory + old + new; 3. all codecs
        BluetoothCodecConfig[] minimumCodecsArray;
        if (!oldCodecConfig.isMandatoryCodec() && !newCodecConfigsArray[0].isMandatoryCodec()) {
            BluetoothCodecConfig[] optionalCodecsArray;
            if (oldCodecType != newCodecType) {
                optionalCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(oldCodecType),
                        getCodecCapabilitiesByType(newCodecType)
                };
                minimumCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC),
                        getCodecCapabilitiesByType(oldCodecType),
                        getCodecCapabilitiesByType(newCodecType)
                };
            } else {
                optionalCodecsArray = new BluetoothCodecConfig[]
                        {getCodecCapabilitiesByType(oldCodecType)};
                minimumCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC),
                        getCodecCapabilitiesByType(oldCodecType)
                };
            }
            BluetoothCodecStatus codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                                                        sCodecCapabilities,
                                                                        optionalCodecsArray);
            mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                      codecStatus,
                                                      newCodecConfigsArray[0]);
            // no mandatory codec in selectable, and should not apply
            verify(mA2dpNativeInterface, times(0)).setCodecConfigPreference(mTestDevice,
                                                                            newCodecConfigsArray);


        } else {
            if (oldCodecType != newCodecType) {
                minimumCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(oldCodecType),
                        getCodecCapabilitiesByType(newCodecType)
                };
            } else {
                minimumCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(oldCodecType),
                };
            }
        }

        // 2. mandatory + old + new codecs only
        BluetoothCodecStatus codecStatus =
                new BluetoothCodecStatus(oldCodecConfig, sCodecCapabilities, minimumCodecsArray);
        mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                  codecStatus,
                                                  newCodecConfigsArray[0]);
        verify(mA2dpNativeInterface, times(invokeNative ? 1 : 0))
                .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

        // 3. all codecs were selectable
        codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                               sCodecCapabilities,
                                               sCodecCapabilities);
        mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                  codecStatus,
                                                  newCodecConfigsArray[0]);
        verify(mA2dpNativeInterface, times(invokeNative ? 2 : 0))
                .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);
    }

    private void testCodecSpecificParametersChangeHelper(int newCodecType, int newCodecSpecific,
            int oldCodecType, int oldCodecSpecific, boolean invokeNative) {
        BluetoothCodecConfig codecDefaultTemp =
                getDefaultCodecConfigByType(oldCodecType, PRIORITY_HIGH);
        BluetoothCodecConfig oldCodecConfig =
                new BluetoothCodecConfig(codecDefaultTemp.getCodecType(),
                                         codecDefaultTemp.getCodecPriority(),
                                         codecDefaultTemp.getSampleRate(),
                                         codecDefaultTemp.getBitsPerSample(),
                                         codecDefaultTemp.getChannelMode(),
                                         oldCodecSpecific, 0, 0, 0);       // Codec-specific fields
        codecDefaultTemp = getDefaultCodecConfigByType(newCodecType, PRIORITY_HIGH);
        BluetoothCodecConfig[] newCodecConfigsArray = new BluetoothCodecConfig[] {
                new BluetoothCodecConfig(codecDefaultTemp.getCodecType(),
                                         codecDefaultTemp.getCodecPriority(),
                                         codecDefaultTemp.getSampleRate(),
                                         codecDefaultTemp.getBitsPerSample(),
                                         codecDefaultTemp.getChannelMode(),
                                         newCodecSpecific, 0, 0, 0)       // Codec-specific fields
        };
        BluetoothCodecStatus codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                                                    sCodecCapabilities,
                                                                    sCodecCapabilities);
        mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                  codecStatus,
                                                  newCodecConfigsArray[0]);
        verify(mA2dpNativeInterface, times(invokeNative ? 1 : 0))
                .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);
    }

    private void testCodecPriorityChangeHelper(int newCodecType, int newCodecPriority,
            int oldCodecType, int oldCodecPriority, boolean shouldApplyWhenAllSelectable) {

        BluetoothCodecConfig[] newCodecConfigsArray =
                new BluetoothCodecConfig[] {
                        getDefaultCodecConfigByType(newCodecType, newCodecPriority)
                };
        BluetoothCodecConfig oldCodecConfig = getDefaultCodecConfigByType(oldCodecType,
                                                                          oldCodecPriority);

        // Test cases: 1. no mandatory; 2. no new codec; 3. mandatory + old + new; 4. all codecs
        BluetoothCodecConfig[] minimumCodecsArray;
        boolean isMinimumCodecsArraySelectable;
        if (!oldCodecConfig.isMandatoryCodec()) {
            if (oldCodecType == newCodecType || newCodecConfigsArray[0].isMandatoryCodec()) {
                // selectable: {-mandatory, +oldCodec = newCodec}, or
                // selectable: {-mandatory = newCodec, +oldCodec}. Not applied
                BluetoothCodecConfig[] poorCodecsArray = new BluetoothCodecConfig[]
                        {getCodecCapabilitiesByType(oldCodecType)};
                BluetoothCodecStatus codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                                                            sCodecCapabilities,
                                                                            poorCodecsArray);
                mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                          codecStatus,
                                                          newCodecConfigsArray[0]);
                verify(mA2dpNativeInterface, times(0))
                        .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

                // selectable: {+mandatory, +oldCodec = newCodec}, or
                // selectable: {+mandatory = newCodec, +oldCodec}.
                minimumCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC),
                        getCodecCapabilitiesByType(oldCodecType)
                };
            } else {
                // selectable: {-mandatory, +oldCodec, +newCodec}. Not applied
                BluetoothCodecConfig[] poorCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(oldCodecType),
                        getCodecCapabilitiesByType(newCodecType)
                };
                BluetoothCodecStatus codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                                                            sCodecCapabilities,
                                                                            poorCodecsArray);
                mA2dpCodecConfig.setCodecConfigPreference(
                        mTestDevice, codecStatus, newCodecConfigsArray[0]);
                verify(mA2dpNativeInterface, times(0))
                        .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

                // selectable: {+mandatory, +oldCodec, -newCodec}. Not applied
                poorCodecsArray = new BluetoothCodecConfig[] {
                        getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC),
                        getCodecCapabilitiesByType(oldCodecType)
                };
                codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                                       sCodecCapabilities,
                                                       poorCodecsArray);
                mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                          codecStatus,
                                                          newCodecConfigsArray[0]);
                verify(mA2dpNativeInterface, times(0))
                        .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

                // selectable: {+mandatory, +oldCodec, +newCodec}.
                minimumCodecsArray = new BluetoothCodecConfig[] {
                      getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC),
                      getCodecCapabilitiesByType(oldCodecType),
                      getCodecCapabilitiesByType(newCodecType)
                };
            }
            // oldCodec priority should be reset to default, so compare with the default
            if (newCodecConfigsArray[0].getCodecPriority()
                    > getDefaultCodecConfigByType(
                            oldCodecType,
                            BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT).getCodecPriority()
                    && oldCodecType != newCodecType) {
                isMinimumCodecsArraySelectable = true;
            } else {
                // the old codec was still the highest priority after reset to default
                isMinimumCodecsArraySelectable = false;
            }
        } else if (oldCodecType != newCodecType) {
            // selectable: {+mandatory = oldCodec, -newCodec}. Not applied
            BluetoothCodecConfig[] poorCodecsArray = new BluetoothCodecConfig[]
                    {getCodecCapabilitiesByType(oldCodecType)};
            BluetoothCodecStatus codecStatus =
                    new BluetoothCodecStatus(oldCodecConfig, sCodecCapabilities, poorCodecsArray);
            mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                      codecStatus,
                                                      newCodecConfigsArray[0]);
            verify(mA2dpNativeInterface, times(0))
                    .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

            // selectable: {+mandatory = oldCodec, +newCodec}.
            minimumCodecsArray = new BluetoothCodecConfig[] {
                    getCodecCapabilitiesByType(oldCodecType),
                    getCodecCapabilitiesByType(newCodecType)
            };
            isMinimumCodecsArraySelectable = true;
        } else {
            // selectable: {mandatory = oldCodec = newCodec}.
            minimumCodecsArray = new BluetoothCodecConfig[]
                    {getCodecCapabilitiesByType(BluetoothCodecConfig.SOURCE_CODEC_TYPE_SBC)};
            isMinimumCodecsArraySelectable = false;
        }

        // 3. mandatory + old + new codecs only
        int invokedCounter = (isMinimumCodecsArraySelectable ? 1 : 0);
        BluetoothCodecStatus codecStatus =
                new BluetoothCodecStatus(oldCodecConfig, sCodecCapabilities, minimumCodecsArray);
        mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                  codecStatus,
                                                  newCodecConfigsArray[0]);
        verify(mA2dpNativeInterface, times(invokedCounter))
                .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);

        // 4. all codecs were selectable
        invokedCounter += (shouldApplyWhenAllSelectable ? 1 : 0);
        codecStatus = new BluetoothCodecStatus(oldCodecConfig,
                                               sCodecCapabilities,
                                               sCodecCapabilities);
        mA2dpCodecConfig.setCodecConfigPreference(mTestDevice,
                                                  codecStatus,
                                                  newCodecConfigsArray[0]);
        verify(mA2dpNativeInterface, times(invokedCounter))
                .setCodecConfigPreference(mTestDevice, newCodecConfigsArray);
    }
}
