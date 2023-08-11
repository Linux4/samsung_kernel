/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.bluetooth.le_audio;

import android.bluetooth.BluetoothLeAudioCodecConfig;
import android.content.Context;
import android.media.AudioManager;
import android.util.Log;
/*
 * LeAudio Codec Configuration setup.
 */
class LeAudioCodecConfig {
    private static final boolean DBG = true;
    private static final String TAG = "LeAudioCodecConfig";

    private Context mContext;
    private BluetoothLeAudioCodecConfig[] mCodecConfigOffloading =
            new BluetoothLeAudioCodecConfig[0];

    LeAudioCodecConfig(Context context) {
        Log.i(TAG, "LeAudioCodecConfig init");
        mContext = context;

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (audioManager == null) {
            Log.w(TAG, "Can't obtain the codec offloading prefernece from null AudioManager");
            return;
        }

        mCodecConfigOffloading = audioManager.getHwOffloadFormatsSupportedForLeAudio()
                                             .toArray(mCodecConfigOffloading);

        if (DBG) {
            Log.i(TAG, "mCodecConfigOffloading size for le -> " + mCodecConfigOffloading.length);

            for (int idx = 0; idx < mCodecConfigOffloading.length; idx++) {
                Log.i(TAG, String.format("mCodecConfigOffloading[%d] -> %s",
                        idx, mCodecConfigOffloading[idx].toString()));
            }
        }
    }

    BluetoothLeAudioCodecConfig[] getCodecConfigOffloading() {
        return mCodecConfigOffloading;
    }
}

