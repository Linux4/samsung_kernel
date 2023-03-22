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

package com.android.bluetooth.btservice;

import android.annotation.NonNull;
import android.annotation.Nullable;

final class DiscoveringPackage {
    private @NonNull String mPackageName;
    private @Nullable String mPermission;
    private boolean mHasDisavowedLocation;

    DiscoveringPackage(@NonNull String packageName, @Nullable String permission,
            boolean hasDisavowedLocation) {
        mPackageName = packageName;
        mPermission = permission;
        mHasDisavowedLocation = hasDisavowedLocation;
    }

    public @NonNull String getPackageName() {
        return mPackageName;
    }

    public @Nullable String getPermission() {
        return mPermission;
    }

    public boolean hasDisavowedLocation() {
        return mHasDisavowedLocation;
    }
}
