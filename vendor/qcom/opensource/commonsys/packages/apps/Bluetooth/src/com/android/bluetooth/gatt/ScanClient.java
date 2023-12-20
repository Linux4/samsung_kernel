/*
 * Copyright (C) 2013 The Android Open Source Project
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

import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.os.Binder;
import android.os.UserHandle;

import java.util.List;
import java.util.Objects;

/**
 * Helper class identifying a client that has requested LE scan results.
 *
 * @hide
 */
/* package */class ScanClient {
    public int scannerId;
    public ScanSettings settings;
    public int scanModeApp;
    public boolean started = false;
    public int appUid;
    public List<ScanFilter> filters;
    // App associated with the scan client died.
    public boolean appDied;
    public boolean hasLocationPermission;
    public UserHandle userHandle;
    public boolean isQApp;
    public boolean eligibleForSanitizedExposureNotification;
    public boolean hasNetworkSettingsPermission;
    public boolean hasNetworkSetupWizardPermission;
    public boolean hasScanWithoutLocationPermission;
    public boolean hasDisavowedLocation;
    public List<String> associatedDevices;
    public boolean allowAddressTypeInResults;
    public String callingPackage;

    public AppScanStats stats = null;

    private static final ScanSettings DEFAULT_SCAN_SETTINGS =
            new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();

    ScanClient(int scannerId) {
        this(scannerId, DEFAULT_SCAN_SETTINGS, null);
    }

    ScanClient(int scannerId, ScanSettings settings, List<ScanFilter> filters) {
        this.scannerId = scannerId;
        this.settings = settings;
        this.scanModeApp = settings.getScanMode();
        this.filters = filters;
        this.appUid = Binder.getCallingUid();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || getClass() != obj.getClass()) {
            return false;
        }
        ScanClient other = (ScanClient) obj;
        return scannerId == other.scannerId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(scannerId);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(" [ScanClient")
                .append(" scanModeApp ").append(scanModeApp)
                .append(" scanModeUsed ").append(settings.getScanMode());
        if (stats != null && stats.appName != null) {
            sb.append(" [appScanStats ").append(stats.appName).append("]");
        }
        sb.append("]");
        return sb.toString();
    }

    /**
     * Update scan settings with the new scan mode.
     * @param newScanMode
     * @return true if scan settings are updated, false otherwise.
     */
    public boolean updateScanMode(int newScanMode) {
        if (settings.getScanMode() == newScanMode) {
            return false;
        }

        ScanSettings.Builder builder = new ScanSettings.Builder();
        settings = builder.setScanMode(newScanMode)
                .setCallbackType(settings.getCallbackType())
                .setScanResultType(settings.getScanResultType())
                .setReportDelay(settings.getReportDelayMillis())
                .setNumOfMatches(settings.getNumOfMatches())
                .setMatchMode(settings.getMatchMode())
                .setLegacy(settings.getLegacy())
                .setPhy(settings.getPhy())
                .build();
        return true;
    }
}
