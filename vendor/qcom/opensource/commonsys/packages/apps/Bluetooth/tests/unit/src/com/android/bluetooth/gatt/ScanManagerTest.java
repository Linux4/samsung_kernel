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

package com.android.bluetooth.gatt;

import static android.bluetooth.le.ScanSettings.SCAN_MODE_OPPORTUNISTIC;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_LOW_POWER;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_BALANCED;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_LOW_LATENCY;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_SCREEN_OFF;
import static android.bluetooth.le.ScanSettings.SCAN_MODE_SCREEN_OFF_BALANCED;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Binder;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.util.SparseIntArray;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ServiceTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.TestUtils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.BluetoothAdapterProxy;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Test cases for {@link ScanManager}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class ScanManagerTest {
    private static final String TAG = ScanManagerTest.class.getSimpleName();
    private static final int DELAY_ASYNC_MS = 10;
    private static final int DELAY_DEFAULT_SCAN_TIMEOUT_MS = 1500000;
    private static final int DELAY_SCAN_TIMEOUT_MS = 100;
    private static final int DEFAULT_SCAN_REPORT_DELAY_MS = 100;
    private static final int DEFAULT_NUM_OFFLOAD_SCAN_FILTER = 16;
    private static final int DEFAULT_BYTES_OFFLOAD_SCAN_RESULT_STORAGE = 4096;

    private Context mTargetContext;
    private GattService mService;
    private ScanManager mScanManager;
    private Handler mHandler;
    private CountDownLatch mLatch;

    @Rule public final ServiceTestRule mServiceRule = new ServiceTestRule();
    @Mock private AdapterService mAdapterService;
    @Mock private BluetoothAdapterProxy mBluetoothAdapterProxy;

    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when GattService is not enabled"
                , GattService.isEnabled());
        MockitoAnnotations.initMocks(this);

        TestUtils.setAdapterService(mAdapterService);
        doReturn(true).when(mAdapterService).isStartedProfile(anyString());
        when(mAdapterService.getScanTimeoutMillis()).
                thenReturn((long)DELAY_DEFAULT_SCAN_TIMEOUT_MS);
        when(mAdapterService.getNumOfOffloadedScanFilterSupported())
                .thenReturn(DEFAULT_NUM_OFFLOAD_SCAN_FILTER);
        when(mAdapterService.getOffloadedScanResultStorage())
                .thenReturn(DEFAULT_BYTES_OFFLOAD_SCAN_RESULT_STORAGE);

        BluetoothAdapterProxy.setInstanceForTesting(mBluetoothAdapterProxy);
        // TODO: Need to handle Native call/callback for hw filter configuration when return true
        when(mBluetoothAdapterProxy.isOffloadedScanFilteringSupported()).thenReturn(false);

        TestUtils.startService(mServiceRule, GattService.class);
        mService = GattService.getGattService();
        assertThat(mService).isNotNull();

        mScanManager = mService.getScanManager();
        assertThat(mScanManager).isNotNull();

        mHandler = mScanManager.getClientHandler();
        assertThat(mHandler).isNotNull();

        mLatch = new CountDownLatch(1);
        assertThat(mLatch).isNotNull();
    }

    @After
    public void tearDown() throws Exception {
        if (!GattService.isEnabled()) {
            return;
        }
        doReturn(false).when(mAdapterService).isStartedProfile(anyString());
        TestUtils.stopService(mServiceRule, GattService.class);
        mService = GattService.getGattService();
        assertThat(mService).isNull();
        TestUtils.clearAdapterService(mAdapterService);
        BluetoothAdapterProxy.setInstanceForTesting(null);
    }

    private void testSleep(long millis) {
        try {
            mLatch.await(millis, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            Log.e(TAG, "Latch await", e);
        }
    }

    private void sendMessageWaitForProcessed(Message msg) {
        if (mHandler == null) {
            Log.e(TAG, "sendMessage: mHandler is null.");
            return;
        }
        mHandler.sendMessage(msg);
        // Wait for async work from handler thread
        TestUtils.waitForLooperToBeIdle(mHandler.getLooper());
    }

    private ScanClient createScanClient(int id, boolean isFiltered, int scanMode,
            boolean isBatch) {
        List<ScanFilter> scanFilterList = createScanFilterList(isFiltered);
        ScanSettings scanSettings = createScanSettings(scanMode, isBatch);

        ScanClient client = new ScanClient(id, scanSettings, scanFilterList);
        client.stats = new AppScanStats("Test", null, null, mService);
        client.stats.recordScanStart(scanSettings, scanFilterList, isFiltered, false, id);
        return client;
    }

    private ScanClient createScanClient(int id, boolean isFiltered, int scanMode) {
        return createScanClient(id, isFiltered, scanMode, false);
    }

    private List<ScanFilter> createScanFilterList(boolean isFiltered) {
        List<ScanFilter> scanFilterList = null;
        if (isFiltered) {
            scanFilterList = new ArrayList<>();
            scanFilterList.add(new ScanFilter.Builder().setDeviceName("TestName").build());
        }
        return scanFilterList;
    }

    private ScanSettings createScanSettings(int scanMode, boolean isBatch) {

        ScanSettings scanSettings = null;
        if(isBatch) {
            scanSettings = new ScanSettings.Builder().setScanMode(scanMode)
                    .setReportDelay(DEFAULT_SCAN_REPORT_DELAY_MS).build();
        } else {
            scanSettings = new ScanSettings.Builder().setScanMode(scanMode).build();
        }
        return scanSettings;
    }

    private Message createStartStopScanMessage(boolean isStartScan, Object obj) {
        Message message = new Message();
        message.what = isStartScan ? ScanManager.MSG_START_BLE_SCAN : ScanManager.MSG_STOP_BLE_SCAN;
        message.obj = obj;
        return message;
    }

    private Message createScreenOnOffMessage(boolean isScreenOn) {
        Message message = new Message();
        message.what = isScreenOn ? ScanManager.MSG_SCREEN_ON : ScanManager.MSG_SCREEN_OFF;
        message.obj = null;
        return message;
    }

    private Message createImportanceMessage(boolean isForeground) {
        final int importance = isForeground ? ActivityManager.RunningAppProcessInfo
                .IMPORTANCE_FOREGROUND_SERVICE : ActivityManager.RunningAppProcessInfo
                .IMPORTANCE_FOREGROUND_SERVICE + 1;
        final int uid = Binder.getCallingUid();
        Message message = new Message();
        message.what = ScanManager.MSG_IMPORTANCE_CHANGE;
        message.obj = new ScanManager.UidImportance(uid, importance);
        return message;
    }

    @Test
    public void testScreenOffStartUnfilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = false;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_AMBIENT_DISCOVERY);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isFalse();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isTrue();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testScreenOffStartFilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = true;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_SCREEN_OFF);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_SCREEN_OFF_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_SCREEN_OFF_BALANCED);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testScreenOnStartUnfilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = false;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_AMBIENT_DISCOVERY);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testScreenOnStartFilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = true;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_AMBIENT_DISCOVERY);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testResumeUnfilteredScanAfterScreenOn() {
        // Set filtered scan flag
        final boolean isFiltered = false;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_SCREEN_OFF);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_SCREEN_OFF_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_SCREEN_OFF_BALANCED);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isFalse();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isTrue();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
        }
    }

    @Test
    public void testResumeFilteredScanAfterScreenOn() {
        // Set filtered scan flag
        final boolean isFiltered = true;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_SCREEN_OFF);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_SCREEN_OFF_BALANCED);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_LATENCY);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_SCREEN_OFF_BALANCED);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
        }
    }

    @Test
    public void testUnfilteredScanTimeout() {
        // Set filtered scan flag
        final boolean isFiltered = false;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_OPPORTUNISTIC);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_OPPORTUNISTIC);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_OPPORTUNISTIC);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_OPPORTUNISTIC);
        // Set scan timeout through Mock
        when(mAdapterService.getScanTimeoutMillis()).thenReturn((long)DELAY_SCAN_TIMEOUT_MS);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
            // Wait for scan timeout
            testSleep(DELAY_SCAN_TIMEOUT_MS + DELAY_ASYNC_MS);
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            assertThat(client.stats.isScanTimeout(client.scannerId)).isTrue();
            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as backgournd app
            sendMessageWaitForProcessed(createImportanceMessage(false));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as foreground app
            sendMessageWaitForProcessed(createImportanceMessage(true));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testFilteredScanTimeout() {
        // Set filtered scan flag
        final boolean isFiltered = true;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_LOW_POWER);
        // Set scan timeout through Mock
        when(mAdapterService.getScanTimeoutMillis()).thenReturn((long)DELAY_SCAN_TIMEOUT_MS);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
            // Wait for scan timeout
            testSleep(DELAY_SCAN_TIMEOUT_MS + DELAY_ASYNC_MS);
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            assertThat(client.stats.isScanTimeout(client.scannerId)).isTrue();
            // Turn off screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(false));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as backgournd app
            sendMessageWaitForProcessed(createImportanceMessage(false));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as foreground app
            sendMessageWaitForProcessed(createImportanceMessage(true));
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
        }
    }

    @Test
    public void testSwitchForeBackgroundUnfilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = false;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_LOW_POWER);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
            // Set as backgournd app
            sendMessageWaitForProcessed(createImportanceMessage(false));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as foreground app
            sendMessageWaitForProcessed(createImportanceMessage(true));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
        }
    }

    @Test
    public void testSwitchForeBackgroundFilteredScan() {
        // Set filtered scan flag
        final boolean isFiltered = true;
        // Set scan mode map {original scan mode (ScanMode) : expected scan mode (expectedScanMode)}
        SparseIntArray scanModeMap = new SparseIntArray();
        scanModeMap.put(SCAN_MODE_LOW_POWER, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_BALANCED, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_LOW_LATENCY, SCAN_MODE_LOW_POWER);
        scanModeMap.put(SCAN_MODE_AMBIENT_DISCOVERY, SCAN_MODE_LOW_POWER);

        for (int i = 0; i < scanModeMap.size(); i++) {
            int ScanMode = scanModeMap.keyAt(i);
            int expectedScanMode = scanModeMap.get(ScanMode);
            Log.d(TAG, "ScanMode: " + String.valueOf(ScanMode)
                    + " expectedScanMode: " + String.valueOf(expectedScanMode));

            // Turn on screen
            sendMessageWaitForProcessed(createScreenOnOffMessage(true));
            // Create scan client
            ScanClient client = createScanClient(i, isFiltered, ScanMode);
            // Start scan
            sendMessageWaitForProcessed(createStartStopScanMessage(true, client));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
            // Set as backgournd app
            sendMessageWaitForProcessed(createImportanceMessage(false));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(expectedScanMode);
            // Set as foreground app
            sendMessageWaitForProcessed(createImportanceMessage(true));
            assertThat(mScanManager.getRegularScanQueue().contains(client)).isTrue();
            assertThat(mScanManager.getSuspendedScanQueue().contains(client)).isFalse();
            assertThat(client.settings.getScanMode()).isEqualTo(ScanMode);
        }
    }
}
