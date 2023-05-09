/*
 * Copyright 2018 The Android Open Source Project
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
package com.android.bluetooth.a2dpsink;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ServiceTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.TestUtils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.storage.DatabaseManager;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class A2dpSinkServiceTest {
    private A2dpSinkService mService = null;
    private BluetoothAdapter mAdapter = null;
    private Context mTargetContext;

    @Rule public final ServiceTestRule mServiceRule = new ServiceTestRule();

    @Mock private AdapterService mAdapterService;
    @Mock private DatabaseManager mDatabaseManager;

    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when A2dpSinkService is not enabled",
                mTargetContext.getResources().getBoolean(R.bool.profile_supported_a2dp_sink));
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAdapterService);
        doReturn(mDatabaseManager).when(mAdapterService).getDatabase();
        TestUtils.startService(mServiceRule, A2dpSinkService.class);
        mService = A2dpSinkService.getA2dpSinkService();
        Assert.assertNotNull(mService);
        // Try getting the Bluetooth adapter
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        Assert.assertNotNull(mAdapter);
    }

    @After
    public void tearDown() throws Exception {
        if (!mTargetContext.getResources().getBoolean(R.bool.profile_supported_a2dp_sink)) {
            return;
        }
        TestUtils.stopService(mServiceRule, A2dpSinkService.class);
        mService = A2dpSinkService.getA2dpSinkService();
        Assert.assertNull(mService);
        TestUtils.clearAdapterService(mAdapterService);
    }

    private BluetoothDevice makeBluetoothDevice(String address) {
        return mAdapter.getRemoteDevice(address);
    }

    /**
     * Mock the priority of a bluetooth device
     *
     * @param device - The bluetooth device you wish to mock the priority of
     * @param priority - The priority value you want the device to have
     */
    private void mockDevicePriority(BluetoothDevice device, int priority) {
        when(mDatabaseManager.getProfileConnectionPolicy(device, BluetoothProfile.A2DP_SINK))
                .thenReturn(priority);
    }

    @Test
    public void testInitialize() {
        Assert.assertNotNull(A2dpSinkService.getA2dpSinkService());
    }

    /**
     * Test that a PRIORITY_ON device is connected to
     */
    @Test
    public void testConnect() {
        BluetoothDevice device = makeBluetoothDevice("11:11:11:11:11:11");
        mockDevicePriority(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        Assert.assertTrue(mService.connect(device));
    }

    /**
     * Test that a PRIORITY_OFF device is not connected to
     */
    @Test
    public void testConnectPriorityOffDevice() {
        BluetoothDevice device = makeBluetoothDevice("11:11:11:11:11:11");
        mockDevicePriority(device, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);
        Assert.assertFalse(mService.connect(device));
    }
}
