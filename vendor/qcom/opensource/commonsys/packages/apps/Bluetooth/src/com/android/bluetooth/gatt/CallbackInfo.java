/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.bluetooth.gatt;

/**
 * Helper class that keeps track of callback parameters for app callbacks.
 * These are held during congestion and reported when congestion clears.
 * @hide
 */
/*package*/

class CallbackInfo {
    public String address;
    public int status;
    public int handle;
    public byte[] value;

    static class Builder {
        private String mAddress;
        private int mStatus;
        private int mHandle;
        private byte[] mValue;

        Builder(String address, int status) {
            mAddress = address;
            mStatus = status;
        }

        Builder setHandle(int handle) {
            mHandle = handle;
            return this;
        }

        Builder setValue(byte[] value) {
            mValue = value;
            return this;
        }

        CallbackInfo build() {
            return new CallbackInfo(mAddress, mStatus, mHandle, mValue);
        }
    }

    private CallbackInfo(String address, int status, int handle, byte[] value) {
        this.address = address;
        this.status = status;
        this.handle = handle;
    }
}

