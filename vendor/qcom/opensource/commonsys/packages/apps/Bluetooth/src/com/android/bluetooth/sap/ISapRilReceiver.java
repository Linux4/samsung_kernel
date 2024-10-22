/*
 * Copyright (C) 2022 The Android Open Source Project
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

package com.android.bluetooth.sap;

import android.hardware.radio.sap.ISap;

/**
 * ISapRilReceiver is used to send messages
 */
public interface ISapRilReceiver extends ISap {
    /**
     * Set mSapProxy to null
     */
    void resetSapProxy();

    /**
     * Notify SapServer that this class is ready for shutdown.
     */
    void notifyShutdown();

    /**
     * Notify SapServer that the RIL socket is connected
     */
    void sendRilConnectMessage();

    /**
     * Get mSapProxyLock
     */
    Object getSapProxyLock();

    /**
     * Verifies mSapProxy is not null
     */
    boolean isProxyValid();
}
