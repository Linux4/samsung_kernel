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

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.HandlerThread;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class CompanionManagerTest {

    private static final String TEST_DEVICE = "11:22:33:44:55:66";

    private AdapterProperties mAdapterProperties;
    private Context mTargetContext;
    private CompanionManager mCompanionManager;

    private HandlerThread mHandlerThread;

    @Mock
    private AdapterService mAdapterService;
    @Mock
    SharedPreferences mSharedPreferences;
    @Mock
    SharedPreferences.Editor mEditor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mTargetContext = InstrumentationRegistry.getTargetContext();
        // Prepare the TestUtils
        TestUtils.setAdapterService(mAdapterService);
        // Start handler thread for this test
        mHandlerThread = new HandlerThread("CompanionManagerTestHandlerThread");
        mHandlerThread.start();
        // Mock the looper
        doReturn(mHandlerThread.getLooper()).when(mAdapterService).getMainLooper();
        // Mock SharedPreferences
        when(mSharedPreferences.edit()).thenReturn(mEditor);
        doReturn(mSharedPreferences).when(mAdapterService).getSharedPreferences(eq(
                CompanionManager.COMPANION_INFO), eq(Context.MODE_PRIVATE));
        // Tell the AdapterService that it is a mock (see isMock documentation)
        doReturn(true).when(mAdapterService).isMock();
        // Use the resources in the instrumentation instead of the mocked AdapterService
        when(mAdapterService.getResources()).thenReturn(mTargetContext.getResources());

        // Must be called to initialize services
        mCompanionManager = new CompanionManager(mAdapterService, null);
    }

    @After
    public void tearDown() throws Exception {
        mHandlerThread.quit();
        TestUtils.clearAdapterService(mAdapterService);
    }

    @Test
    public void testLoadCompanionInfo_hasCompanionDeviceKey() {
        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_PRIMARY);
    }

    @Test
    public void testLoadCompanionInfo_noCompanionDeviceSetButHaveBondedDevices_shouldNotCrash() {
        BluetoothDevice[] devices = new BluetoothDevice[2];
        doReturn(devices).when(mAdapterService).getBondedDevices();
        doThrow(new IllegalArgumentException())
                .when(mSharedPreferences)
                .getInt(eq(CompanionManager.COMPANION_TYPE_KEY), anyInt());
        mCompanionManager.loadCompanionInfo();
    }

    @Test
    public void testIsCompanionDevice() {
        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_NONE);
        Assert.assertTrue(mCompanionManager.isCompanionDevice(TEST_DEVICE));

        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_PRIMARY);
        Assert.assertTrue(mCompanionManager.isCompanionDevice(TEST_DEVICE));

        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_SECONDARY);
        Assert.assertTrue(mCompanionManager.isCompanionDevice(TEST_DEVICE));
    }

    @Test
    public void testGetGattConnParameterPrimary() {
        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_PRIMARY);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER);

        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_SECONDARY);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER);

        loadCompanionInfoHelper(TEST_DEVICE, CompanionManager.COMPANION_TYPE_NONE);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_HIGH);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
        checkReasonableConnParameterHelper(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER);
    }

    private void loadCompanionInfoHelper(String address, int companionType) {
        doReturn(address)
                .when(mSharedPreferences)
                .getString(eq(CompanionManager.COMPANION_DEVICE_KEY), anyString());
        doReturn(companionType)
                .when(mSharedPreferences)
                .getInt(eq(CompanionManager.COMPANION_TYPE_KEY), anyInt());
        mCompanionManager.loadCompanionInfo();
    }

    private void checkReasonableConnParameterHelper(int priority) {
        // Max/Min values from the Bluetooth spec Version 5.3 | Vol 4, Part E | 7.8.18
        final int minInterval = 6;    // 0x0006
        final int maxInterval = 3200; // 0x0C80
        final int minLatency = 0;     // 0x0000
        final int maxLatency = 499;   // 0x01F3

        int min = mCompanionManager.getGattConnParameters(
                TEST_DEVICE, CompanionManager.GATT_CONN_INTERVAL_MIN,
                priority);
        int max = mCompanionManager.getGattConnParameters(
                TEST_DEVICE, CompanionManager.GATT_CONN_INTERVAL_MAX,
                priority);
        int latency = mCompanionManager.getGattConnParameters(
                TEST_DEVICE, CompanionManager.GATT_CONN_LATENCY,
                priority);

        Assert.assertTrue(max >= min);
        Assert.assertTrue(max >= minInterval);
        Assert.assertTrue(min >= minInterval);
        Assert.assertTrue(max <= maxInterval);
        Assert.assertTrue(min <= maxInterval);
        Assert.assertTrue(latency >= minLatency);
        Assert.assertTrue(latency <= maxLatency);
    }
}
