/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.bluetooth.mapclient;

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

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class MapClientTest {
    private static final String TAG = MapClientTest.class.getSimpleName();
    private MapClientService mService = null;
    private BluetoothAdapter mAdapter = null;
    private Context mTargetContext;

    @Mock private AdapterService mAdapterService;
    @Mock private MnsService mMockMnsService;
    @Mock private DatabaseManager mDatabaseManager;

    @Rule public final ServiceTestRule mServiceRule = new ServiceTestRule();

    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when MapClientService is not enabled",
                mTargetContext.getResources().getBoolean(R.bool.profile_supported_mapmce));
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAdapterService);
        when(mAdapterService.getDatabase()).thenReturn(mDatabaseManager);
        MapUtils.setMnsService(mMockMnsService);
        TestUtils.startService(mServiceRule, MapClientService.class);
        mService = MapClientService.getMapClientService();
        Assert.assertNotNull(mService);
        cleanUpInstanceMap();
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    @After
    public void tearDown() throws Exception {
        if (!mTargetContext.getResources().getBoolean(R.bool.profile_supported_mapmce)) {
            return;
        }
        TestUtils.stopService(mServiceRule, MapClientService.class);
        mService = MapClientService.getMapClientService();
        Assert.assertNull(mService);
        TestUtils.clearAdapterService(mAdapterService);
    }

    private void cleanUpInstanceMap() {
        if (!mService.getInstanceMap().isEmpty()) {
            List<BluetoothDevice> deviceList = mService.getConnectedDevices();
            for (BluetoothDevice d : deviceList) {
                mService.disconnect(d);
            }
        }
        Assert.assertTrue(mService.getInstanceMap().isEmpty());
    }

    /**
     * Mock the priority of a bluetooth device
     *
     * @param device - The bluetooth device you wish to mock the priority of
     * @param priority - The priority value you want the device to have
     */
    private void mockDevicePriority(BluetoothDevice device, int priority) {
        when(mDatabaseManager.getProfileConnectionPolicy(device, BluetoothProfile.MAP_CLIENT))
                .thenReturn(priority);
    }

    @Test
    public void testInitialize() {
        Assert.assertNotNull(MapClientService.getMapClientService());
    }

    /**
     * Test connection of one device.
     */
    @Test
    public void testConnect() {
        // make sure there is no statemachine already defined for this device
        BluetoothDevice device = makeBluetoothDevice("11:11:11:11:11:11");
        Assert.assertNull(mService.getInstanceMap().get(device));

        // connect a bluetooth device
        mockDevicePriority(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        Assert.assertTrue(mService.connect(device));

        // is the statemachine created
        Map<BluetoothDevice, MceStateMachine> map = mService.getInstanceMap();
        Assert.assertEquals(1, map.size());
        Assert.assertNotNull(map.get(device));
    }

    /**
     * Test that a PRIORITY_OFF device is not connected to
     */
    @Test
    public void testConnectPriorityOffDevice() {
        // make sure there is no statemachine already defined for this device
        BluetoothDevice device = makeBluetoothDevice("11:11:11:11:11:11");
        Assert.assertNull(mService.getInstanceMap().get(device));

        // connect a bluetooth device
        mockDevicePriority(device, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);
        Assert.assertFalse(mService.connect(device));

        // is the statemachine created
        Map<BluetoothDevice, MceStateMachine> map = mService.getInstanceMap();
        Assert.assertEquals(0, map.size());
        Assert.assertNull(map.get(device));
    }

    /**
     * Test connecting MAXIMUM_CONNECTED_DEVICES devices.
     */
    @Test
    public void testConnectMaxDevices() {
        // Create bluetoothdevice & mock statemachine objects to be used in this test
        List<BluetoothDevice> list = new ArrayList<>();
        String address = "11:11:11:11:11:1";
        for (int i = 0; i < MapClientService.MAXIMUM_CONNECTED_DEVICES; ++i) {
            list.add(makeBluetoothDevice(address + i));
        }

        // make sure there is no statemachine already defined for the devices defined above
        for (BluetoothDevice d : list) {
            Assert.assertNull(mService.getInstanceMap().get(d));
        }

        // run the test - connect all devices, set their priorities to on
        for (BluetoothDevice d : list) {
            mockDevicePriority(d, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            Assert.assertTrue(mService.connect(d));
        }

        // verify
        Map<BluetoothDevice, MceStateMachine> map = mService.getInstanceMap();
        Assert.assertEquals(MapClientService.MAXIMUM_CONNECTED_DEVICES, map.size());
        for (BluetoothDevice d : list) {
            Assert.assertNotNull(map.get(d));
        }

        // Try to connect one more device. Should fail.
        BluetoothDevice last = makeBluetoothDevice("11:22:33:44:55:66");
        Assert.assertFalse(mService.connect(last));
    }

    private BluetoothDevice makeBluetoothDevice(String address) {
        return mAdapter.getRemoteDevice(address);
    }
}
