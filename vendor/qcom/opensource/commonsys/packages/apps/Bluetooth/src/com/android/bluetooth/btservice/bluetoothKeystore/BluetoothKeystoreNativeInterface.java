/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.bluetooth.btservice.bluetoothkeystore;

import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import java.io.IOException;
import java.security.NoSuchAlgorithmException;

final class BluetoothKeystoreNativeInterface {

    private static final String TAG = "BluetoothKeystoreNativeInterface";

    @GuardedBy("INSTANCE_LOCK")
    private static BluetoothKeystoreNativeInterface sInstance;
    private static final Object INSTANCE_LOCK = new Object();

    static {
        classInitNative();
    }

    private BluetoothKeystoreNativeInterface() {
    }

    /**
     * Get singleton instance.
     */
    public static BluetoothKeystoreNativeInterface getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new BluetoothKeystoreNativeInterface();
            }
            return sInstance;
        }
    }

    /**
     * Initializes the native interface.
     *
     * priorities to configure.
     */
    public void init() {
        initNative();
    }

    /**
     * Cleanup the native interface.
     */
    public void cleanup() {
        cleanupNative();
    }

    // Callbacks from the native stack back into the Java framework.
    // All callbacks are routed via the Service which will disambiguate which
    // state machine the message should be routed to.

    private void setEncryptKeyOrRemoveKeyCallback(String prefixString, String decryptedString) {
        BluetoothKeystoreService service = BluetoothKeystoreService.getBluetoothKeystoreService();
        if (service != null) {
            try {
                service.setEncryptKeyOrRemoveKey(prefixString, decryptedString);
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted while operating.");
            } catch (IOException e) {
                Log.e(TAG, "IO error while file operating.");
            } catch (NoSuchAlgorithmException e) {
                Log.e(TAG, "encrypt could not find the algorithm: SHA256");
            }
        } else {
            Log.e(TAG, "Event ignored, service not available: " + prefixString);
        }
    }

    private String getKeyCallback(String prefixString) {
        BluetoothKeystoreService service = BluetoothKeystoreService.getBluetoothKeystoreService();
        if (service != null) {
            return service.getKey(prefixString);
        } else {
            Log.e(TAG, "Event ignored, service not available: " + prefixString);
            return null;
        }
    }

    // Native methods that call into the JNI interface
    private static native void classInitNative();
    private native void initNative();
    private native void cleanupNative();
}
