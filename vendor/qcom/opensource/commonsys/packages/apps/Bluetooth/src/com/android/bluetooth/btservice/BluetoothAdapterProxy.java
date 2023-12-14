/*
 * Copyright 2022 The Android Open Source Project
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

import android.bluetooth.BluetoothAdapter;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.internal.annotations.VisibleForTesting;

/**
 * A proxy class that facilitates testing of the ScanManager.
 *
 * This is necessary due to the "final" attribute of the BluetoothAdapter class. In order to
 * test the correct functioning of the ScanManager class, the final class must be put
 * into a container that can be mocked correctly.
 */
public class BluetoothAdapterProxy {
    private static final String TAG = BluetoothAdapterProxy.class.getSimpleName();
    private static BluetoothAdapterProxy sInstance;
    private static final Object INSTANCE_LOCK = new Object();

    private BluetoothAdapterProxy() {}

    /**
     * Get the singleton instance of proxy.
     *
     * @return the singleton instance, guaranteed not null
     */
    public static BluetoothAdapterProxy getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = new BluetoothAdapterProxy();
            }
            return sInstance;
        }
    }

    /**
     * Proxy function that calls {@link BluetoothAdapter#isOffloadedFilteringSupported()}.
     *
     * @return whether the offloaded scan filtering is supported
     */
    public boolean isOffloadedScanFilteringSupported() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return adapter.isOffloadedFilteringSupported();
    }

    /**
     * Allow unit tests to substitute BluetoothAdapterProxy with a test instance
     *
     * @param proxy a test instance of the BluetoothAdapterProxy
     */
    @VisibleForTesting
    public static void setInstanceForTesting(BluetoothAdapterProxy proxy) {
        Utils.enforceInstrumentationTestMode();
        synchronized (INSTANCE_LOCK) {
            Log.d(TAG, "setInstanceForTesting(), set to " + proxy);
            sInstance = proxy;
        }
    }
}
