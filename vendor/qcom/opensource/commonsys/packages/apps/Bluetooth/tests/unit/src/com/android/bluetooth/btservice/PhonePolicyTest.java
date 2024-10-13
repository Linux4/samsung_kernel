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

package com.android.bluetooth.btservice;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.content.Intent;
import android.os.HandlerThread;
import android.os.ParcelUuid;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.TestUtils;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.hfp.HeadsetService;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class PhonePolicyTest {
    private static final int MAX_CONNECTED_AUDIO_DEVICES = 5;
    private static final int ASYNC_CALL_TIMEOUT_MILLIS = 250;
    private static final int CONNECT_OTHER_PROFILES_TIMEOUT_MILLIS = 1000;
    private static final int CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS =
            CONNECT_OTHER_PROFILES_TIMEOUT_MILLIS * 3 / 2;

    private HandlerThread mHandlerThread;
    private BluetoothAdapter mAdapter;
    private PhonePolicy mPhonePolicy;

    @Mock private AdapterService mAdapterService;
    @Mock private ServiceFactory mServiceFactory;
    @Mock private HeadsetService mHeadsetService;
    @Mock private A2dpService mA2dpService;
    @Mock private DatabaseManager mDatabaseManager;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Stub A2DP and HFP
        when(mHeadsetService.connect(any(BluetoothDevice.class))).thenReturn(true);
        when(mA2dpService.connect(any(BluetoothDevice.class))).thenReturn(true);
        // Prepare the TestUtils
        TestUtils.setAdapterService(mAdapterService);
        // Configure the maximum connected audio devices
        doReturn(MAX_CONNECTED_AUDIO_DEVICES).when(mAdapterService).getMaxConnectedAudioDevices();
        doReturn(mDatabaseManager).when(mAdapterService).getDatabase();
        // Setup the mocked factory to return mocked services
        doReturn(mHeadsetService).when(mServiceFactory).getHeadsetService();
        doReturn(mA2dpService).when(mServiceFactory).getA2dpService();
        // Start handler thread for this test
        mHandlerThread = new HandlerThread("PhonePolicyTestHandlerThread");
        mHandlerThread.start();
        // Mock the looper
        doReturn(mHandlerThread.getLooper()).when(mAdapterService).getMainLooper();
        // Tell the AdapterService that it is a mock (see isMock documentation)
        doReturn(true).when(mAdapterService).isMock();
        // Must be called to initialize services
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        PhonePolicy.sConnectOtherProfilesTimeoutMillis = CONNECT_OTHER_PROFILES_TIMEOUT_MILLIS;
        mPhonePolicy = new PhonePolicy(mAdapterService, mServiceFactory);
    }

    @After
    public void tearDown() throws Exception {
        mHandlerThread.quit();
        TestUtils.clearAdapterService(mAdapterService);
    }

    /**
     * Test that when new UUIDs are refreshed for a device then we set the priorities for various
     * profiles accurately. The following profiles should have ON priorities:
     *     A2DP, HFP, HID and PAN
     */
    @Test
    public void testProcessInitProfilePriorities() {
        BluetoothDevice device = TestUtils.getTestDevice(mAdapter, 0);
        // Mock the HeadsetService to return unknown connection policy
        when(mHeadsetService.getConnectionPolicy(device))
                .thenReturn(BluetoothProfile.CONNECTION_POLICY_UNKNOWN);

        // Mock the A2DP service to return undefined unknown connection policy
        when(mA2dpService.getConnectionPolicy(device))
                .thenReturn(BluetoothProfile.CONNECTION_POLICY_UNKNOWN);

        when(mAdapterService.getDatabase()).thenReturn(mDatabaseManager);

        // Inject an event for UUIDs updated for a remote device with only HFP enabled
        Intent intent = new Intent(BluetoothDevice.ACTION_UUID);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        ParcelUuid[] uuids = new ParcelUuid[2];
        uuids[0] = BluetoothUuid.HFP;
        uuids[1] = BluetoothUuid.A2DP_SINK;
        intent.putExtra(BluetoothDevice.EXTRA_UUID, uuids);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Check that the priorities of the devices for preferred profiles are set to ON
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS))
                .setProfileConnectionPolicy(device, BluetoothProfile.HEADSET,
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS))
                .setProfileConnectionPolicy(device, BluetoothProfile.A2DP,
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
    }

    /**
     * Test that when the adapter is turned ON then we call autoconnect on devices that have HFP and
     * A2DP enabled. NOTE that the assumption is that we have already done the pairing previously
     * and hence the priorities for the device is already set to AUTO_CONNECT over HFP and A2DP (as
     * part of post pairing process).
     */
    @Test
    public void testAdapterOnAutoConnect() {
        // Return desired values from the mocked object(s)
        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);
        when(mAdapterService.isQuietModeEnabled()).thenReturn(false);

        // Return a list of connection order
        BluetoothDevice bondedDevice = TestUtils.getTestDevice(mAdapter, 0);
        when(mDatabaseManager.getMostRecentlyConnectedA2dpDevice()).thenReturn(bondedDevice);
        when(mAdapterService.getBondState(bondedDevice)).thenReturn(BluetoothDevice.BOND_BONDED);

        // Return CONNECTION_POLICY_ALLOWED over HFP and A2DP
        when(mHeadsetService.getConnectionPolicy(bondedDevice)).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevice)).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        // Inject an event that the adapter is turned on.
        Intent intent = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        intent.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_ON);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Check that we got a request to connect over HFP and A2DP
        verify(mA2dpService, timeout(ASYNC_CALL_TIMEOUT_MILLIS)).connect(eq(bondedDevice));
        verify(mHeadsetService, timeout(ASYNC_CALL_TIMEOUT_MILLIS)).connect(eq(bondedDevice));
    }

    /**
     * Test that when an active device is disconnected, we will not auto connect it
     */
    @Test
    public void testDisconnectNoAutoConnect() {
        // Return desired values from the mocked object(s)
        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);
        when(mAdapterService.isQuietModeEnabled()).thenReturn(false);

        // Return a list of connection order
        List<BluetoothDevice> connectionOrder = new ArrayList<>();
        connectionOrder.add(TestUtils.getTestDevice(mAdapter, 0));
        connectionOrder.add(TestUtils.getTestDevice(mAdapter, 1));
        connectionOrder.add(TestUtils.getTestDevice(mAdapter, 2));
        connectionOrder.add(TestUtils.getTestDevice(mAdapter, 3));

        when(mDatabaseManager.getMostRecentlyConnectedA2dpDevice()).thenReturn(
                connectionOrder.get(0));

        // Make all devices auto connect
        when(mHeadsetService.getConnectionPolicy(connectionOrder.get(0))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mHeadsetService.getConnectionPolicy(connectionOrder.get(1))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mHeadsetService.getConnectionPolicy(connectionOrder.get(2))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mHeadsetService.getConnectionPolicy(connectionOrder.get(3))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);

        // Make one of the device active
        Intent intent = new Intent(BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, connectionOrder.get(0));
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Only calls setConnection on device connectionOrder.get(0) with STATE_CONNECTED
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS)).setConnection(
                connectionOrder.get(0), true);
        verify(mDatabaseManager, never()).setConnection(eq(connectionOrder.get(1)), anyBoolean());
        verify(mDatabaseManager, never()).setConnection(eq(connectionOrder.get(2)), anyBoolean());
        verify(mDatabaseManager, never()).setConnection(eq(connectionOrder.get(3)), anyBoolean());

        // Make another device active
        when(mHeadsetService.getConnectionState(connectionOrder.get(1))).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        intent = new Intent(BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, connectionOrder.get(1));
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Only calls setConnection on device connectionOrder.get(1) with STATE_CONNECTED
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setConnection(
                connectionOrder.get(0), true);
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setConnection(
                connectionOrder.get(1), true);
        verify(mDatabaseManager, never()).setConnection(eq(connectionOrder.get(2)), anyBoolean());
        verify(mDatabaseManager, never()).setConnection(eq(connectionOrder.get(3)), anyBoolean());

        // Disconnect a2dp for the device
        when(mHeadsetService.getConnectionState(connectionOrder.get(1))).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        intent = new Intent(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, connectionOrder.get(1));
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_CONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_DISCONNECTED);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Verify that we do not call setConnection, but instead setDisconnection on disconnect
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setConnection(
                connectionOrder.get(1), true);
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setDisconnection(
                connectionOrder.get(1));

        // Make the current active device fail to connect
        when(mA2dpService.getConnectionState(connectionOrder.get(1))).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        updateProfileConnectionStateHelper(connectionOrder.get(1), BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_DISCONNECTED, BluetoothProfile.STATE_CONNECTING);

        // Verify we don't call deleteConnection as that only happens when we disconnect a2dp
        verify(mDatabaseManager, timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).setDisconnection(
                connectionOrder.get(1));

        // Verify we didn't have any unexpected calls to setConnection or deleteConnection
        verify(mDatabaseManager, times(2)).setConnection(any(BluetoothDevice.class), anyBoolean());
        verify(mDatabaseManager, times(1)).setDisconnection(any(BluetoothDevice.class));
    }

    /**
     * Test that we will try to re-connect to a profile on a device if other profile(s) are
     * connected. This is to add robustness to the connection mechanism
     */
    @Test
    public void testReconnectOnPartialConnect() {
        // Return a list of bonded devices (just one)
        BluetoothDevice[] bondedDevices = new BluetoothDevice[1];
        bondedDevices[0] = TestUtils.getTestDevice(mAdapter, 0);
        when(mAdapterService.getBondedDevices()).thenReturn(bondedDevices);

        // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles are
        // auto-connectable.
        when(mHeadsetService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);

        // We want to trigger (in CONNECT_OTHER_PROFILES_TIMEOUT) a call to connect A2DP
        // To enable that we need to make sure that HeadsetService returns the device as list of
        // connected devices
        ArrayList<BluetoothDevice> hsConnectedDevices = new ArrayList<>();
        hsConnectedDevices.add(bondedDevices[0]);
        when(mHeadsetService.getConnectedDevices()).thenReturn(hsConnectedDevices);
        // Also the A2DP should say that its not connected for same device
        when(mA2dpService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        updateProfileConnectionStateHelper(bondedDevices[0], BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to A2DP connect
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(bondedDevices[0]));
    }

    /**
     * Test that we will try to re-connect to a profile on a device next time if a previous attempt
     * failed partially. This will make sure the connection mechanism still works at next try while
     * the previous attempt is some profiles connected on a device but some not.
     */
    @Test
    public void testReconnectOnPartialConnect_PreviousPartialFail() {
        List<BluetoothDevice> connectionOrder = new ArrayList<>();
        connectionOrder.add(TestUtils.getTestDevice(mAdapter, 0));
        when(mDatabaseManager.getMostRecentlyConnectedA2dpDevice()).thenReturn(
                connectionOrder.get(0));

        // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles are
        // auto-connectable.
        when(mHeadsetService.getConnectionPolicy(connectionOrder.get(0))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(connectionOrder.get(0))).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);

        // We want to trigger (in CONNECT_OTHER_PROFILES_TIMEOUT) a call to connect A2DP
        // To enable that we need to make sure that HeadsetService returns the device among a list
        // of connected devices
        ArrayList<BluetoothDevice> hsConnectedDevices = new ArrayList<>();
        hsConnectedDevices.add(connectionOrder.get(0));
        when(mHeadsetService.getConnectedDevices()).thenReturn(hsConnectedDevices);
        // Also the A2DP should say that its not connected for same device
        when(mA2dpService.getConnectionState(connectionOrder.get(0))).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection success event for one profile since the re-connect *only* works if
        // we have already connected successfully over one of the profiles
        updateProfileConnectionStateHelper(connectionOrder.get(0), BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to A2DP reconnect
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                connectionOrder.get(0));

        // We send a connection failure event for the attempted profile, and keep the connected
        // profile connected.
        updateProfileConnectionStateHelper(connectionOrder.get(0), BluetoothProfile.A2DP,
                BluetoothProfile.STATE_DISCONNECTED, BluetoothProfile.STATE_CONNECTING);

        TestUtils.waitForLooperToFinishScheduledTask(mHandlerThread.getLooper());

        // Verify no one changes the priority of the failed profile
        verify(mA2dpService, never()).setConnectionPolicy(eq(connectionOrder.get(0)), anyInt());

        // Send a connection success event for one profile again without disconnecting all profiles
        updateProfileConnectionStateHelper(connectionOrder.get(0), BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we won't get a call to A2DP reconnect again before all profiles disconnected
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                connectionOrder.get(0));

        // Send a disconnection event for all connected profiles
        hsConnectedDevices.remove(connectionOrder.get(0));
        updateProfileConnectionStateHelper(connectionOrder.get(0), BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_DISCONNECTED, BluetoothProfile.STATE_CONNECTED);

        TestUtils.waitForLooperToFinishScheduledTask(mHandlerThread.getLooper());

        // Send a connection success event for one profile again to trigger re-connect
        hsConnectedDevices.add(connectionOrder.get(0));
        updateProfileConnectionStateHelper(connectionOrder.get(0), BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to A2DP connect again
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS).times(2)).connect(
                connectionOrder.get(0));
    }

    /**
     * Test that a second device will auto-connect if there is already one connected device.
     *
     * Even though we currently only set one device to be auto connect. The consumer of the auto
     * connect property works independently so that we will connect to all devices that are in
     * auto connect mode.
     */
    @Test
    public void testAutoConnectMultipleDevices() {
        final int kMaxTestDevices = 3;
        BluetoothDevice[] testDevices = new BluetoothDevice[kMaxTestDevices];
        ArrayList<BluetoothDevice> hsConnectedDevices = new ArrayList<>();
        ArrayList<BluetoothDevice> a2dpConnectedDevices = new ArrayList<>();
        BluetoothDevice a2dpNotConnectedDevice1 = null;
        BluetoothDevice a2dpNotConnectedDevice2 = null;

        for (int i = 0; i < kMaxTestDevices; i++) {
            BluetoothDevice testDevice = TestUtils.getTestDevice(mAdapter, i);
            testDevices[i] = testDevice;

            // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles
            // are auto-connectable.
            when(mHeadsetService.getConnectionPolicy(testDevice)).thenReturn(
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            when(mA2dpService.getConnectionPolicy(testDevice)).thenReturn(
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            // We want to trigger (in CONNECT_OTHER_PROFILES_TIMEOUT) a call to connect A2DP
            // To enable that we need to make sure that HeadsetService returns the device as list
            // of connected devices.
            hsConnectedDevices.add(testDevice);
            // Connect A2DP for all devices except the last one
            if (i < (kMaxTestDevices - 2)) {
                a2dpConnectedDevices.add(testDevice);
            }
        }
        a2dpNotConnectedDevice1 = hsConnectedDevices.get(kMaxTestDevices - 1);
        a2dpNotConnectedDevice2 = hsConnectedDevices.get(kMaxTestDevices - 2);

        when(mAdapterService.getBondedDevices()).thenReturn(testDevices);
        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);
        when(mHeadsetService.getConnectedDevices()).thenReturn(hsConnectedDevices);
        when(mA2dpService.getConnectedDevices()).thenReturn(a2dpConnectedDevices);
        // Two of the A2DP devices are not connected
        when(mA2dpService.getConnectionState(a2dpNotConnectedDevice1)).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mA2dpService.getConnectionState(a2dpNotConnectedDevice2)).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        updateProfileConnectionStateHelper(a2dpNotConnectedDevice1, BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        updateProfileConnectionStateHelper(a2dpNotConnectedDevice2, BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to A2DP connect
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(a2dpNotConnectedDevice1));
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(a2dpNotConnectedDevice2));
    }

    /**
     * Test that the connection policy of all devices are set as appropriate if there is one
     * connected device.
     * - The HFP and A2DP connect priority for connected devices is set to
     *   BluetoothProfile.PRIORITY_AUTO_CONNECT
     * - The HFP and A2DP connect priority for bonded devices is set to
     *   BluetoothProfile.CONNECTION_POLICY_ALLOWED
     */
    @Test
    public void testSetConnectionPolicyMultipleDevices() {
        // testDevices[0] - connected for both HFP and A2DP
        // testDevices[1] - connected only for HFP - will auto-connect for A2DP
        // testDevices[2] - connected only for A2DP - will auto-connect for HFP
        // testDevices[3] - not connected
        final int kMaxTestDevices = 4;
        BluetoothDevice[] testDevices = new BluetoothDevice[kMaxTestDevices];
        ArrayList<BluetoothDevice> hsConnectedDevices = new ArrayList<>();
        ArrayList<BluetoothDevice> a2dpConnectedDevices = new ArrayList<>();

        for (int i = 0; i < kMaxTestDevices; i++) {
            BluetoothDevice testDevice = TestUtils.getTestDevice(mAdapter, i);
            testDevices[i] = testDevice;

            // Connect HFP and A2DP for each device as appropriate.
            // Return PRIORITY_AUTO_CONNECT only for testDevices[0]
            if (i == 0) {
                hsConnectedDevices.add(testDevice);
                a2dpConnectedDevices.add(testDevice);
                when(mHeadsetService.getConnectionPolicy(testDevice)).thenReturn(
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                when(mA2dpService.getConnectionPolicy(testDevice)).thenReturn(
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
            if (i == 1) {
                hsConnectedDevices.add(testDevice);
                when(mHeadsetService.getConnectionPolicy(testDevice)).thenReturn(
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                when(mA2dpService.getConnectionPolicy(testDevice))
                        .thenReturn(BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
            if (i == 2) {
                a2dpConnectedDevices.add(testDevice);
                when(mHeadsetService.getConnectionPolicy(testDevice)).thenReturn(
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                when(mA2dpService.getConnectionPolicy(testDevice))
                        .thenReturn(BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
            if (i == 3) {
                // Device not connected
                when(mHeadsetService.getConnectionPolicy(testDevice)).thenReturn(
                        BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                when(mA2dpService.getConnectionPolicy(testDevice))
                        .thenReturn(BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
        }
        when(mAdapterService.getBondedDevices()).thenReturn(testDevices);
        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);
        when(mHeadsetService.getConnectedDevices()).thenReturn(hsConnectedDevices);
        when(mA2dpService.getConnectedDevices()).thenReturn(a2dpConnectedDevices);
        // Some of the devices are not connected
        // testDevices[0] - connected for both HFP and A2DP
        when(mHeadsetService.getConnectionState(testDevices[0])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        when(mA2dpService.getConnectionState(testDevices[0])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        // testDevices[1] - connected only for HFP - will auto-connect for A2DP
        when(mHeadsetService.getConnectionState(testDevices[1])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        when(mA2dpService.getConnectionState(testDevices[1])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        // testDevices[2] - connected only for A2DP - will auto-connect for HFP
        when(mHeadsetService.getConnectionState(testDevices[2])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mA2dpService.getConnectionState(testDevices[2])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        // testDevices[3] - not connected
        when(mHeadsetService.getConnectionState(testDevices[3])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mA2dpService.getConnectionState(testDevices[3])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);


        // Generate connection state changed for HFP for testDevices[1] and trigger
        // auto-connect for A2DP.
        updateProfileConnectionStateHelper(testDevices[1], BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to A2DP connect
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(testDevices[1]));

        // testDevices[1] auto-connect completed for A2DP
        a2dpConnectedDevices.add(testDevices[1]);
        when(mA2dpService.getConnectedDevices()).thenReturn(a2dpConnectedDevices);
        when(mA2dpService.getConnectionState(testDevices[1])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);

        // Check the connect priorities for all devices
        // - testDevices[0] - connected for HFP and A2DP: setConnectionPolicy() should not be called
        // - testDevices[1] - connection state changed for HFP should no longer trigger auto
        //                    connect priority change since it is now triggered by A2DP active
        //                    device change intent
        // - testDevices[2] - connected for A2DP: setConnectionPolicy() should not be called
        // - testDevices[3] - not connected for HFP nor A2DP: setConnectionPolicy() should not be
        //                    called
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[0]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[0]), anyInt());
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[1]),
                eq(BluetoothProfile.PRIORITY_AUTO_CONNECT));
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[1]), anyInt());
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[2]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[2]), anyInt());
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[3]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[3]), anyInt());
        clearInvocations(mHeadsetService, mA2dpService);

        // Generate connection state changed for A2DP for testDevices[2] and trigger
        // auto-connect for HFP.
        updateProfileConnectionStateHelper(testDevices[2], BluetoothProfile.A2DP,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we get a call to HFP connect
        verify(mHeadsetService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(testDevices[2]));

        // testDevices[2] auto-connect completed for HFP
        hsConnectedDevices.add(testDevices[2]);
        when(mHeadsetService.getConnectedDevices()).thenReturn(hsConnectedDevices);
        when(mHeadsetService.getConnectionState(testDevices[2])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);

        // Check the connect priorities for all devices
        // - testDevices[0] - connected for HFP and A2DP: setConnectionPolicy() should not be called
        // - testDevices[1] - connected for HFP and A2DP: setConnectionPolicy() should not be called
        // - testDevices[2] - connection state changed for A2DP should no longer trigger auto
        //                    connect priority change since it is now triggered by A2DP
        //                    active device change intent
        // - testDevices[3] - not connected for HFP nor A2DP: setConnectionPolicy() should not be
        //                    called
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[0]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[0]), anyInt());
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[1]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[1]), anyInt());
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[2]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[2]),
                eq(BluetoothProfile.PRIORITY_AUTO_CONNECT));
        verify(mHeadsetService, times(0)).setConnectionPolicy(eq(testDevices[3]), anyInt());
        verify(mA2dpService, times(0)).setConnectionPolicy(eq(testDevices[3]), anyInt());
        clearInvocations(mHeadsetService, mA2dpService);
    }

    /**
     * Test that we will not try to reconnect on a profile if all the connections failed
     */
    @Test
    public void testNoReconnectOnNoConnect() {
        // Return a list of bonded devices (just one)
        BluetoothDevice[] bondedDevices = new BluetoothDevice[1];
        bondedDevices[0] = TestUtils.getTestDevice(mAdapter, 0);
        when(mAdapterService.getBondedDevices()).thenReturn(bondedDevices);

        // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles are
        // auto-connectable.
        when(mHeadsetService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);

        // Return an empty list simulating that the above connection successful was nullified
        when(mHeadsetService.getConnectedDevices()).thenReturn(Collections.emptyList());
        when(mA2dpService.getConnectedDevices()).thenReturn(Collections.emptyList());

        // Both A2DP and HFP should say this device is not connected, except for the intent
        when(mA2dpService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mHeadsetService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, bondedDevices[0]);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_DISCONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Check that we don't get any calls to reconnect
        verify(mA2dpService, after(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS).never()).connect(
                eq(bondedDevices[0]));
        verify(mHeadsetService, never()).connect(eq(bondedDevices[0]));
    }

    /**
     * Test that we will not try to reconnect on a profile if all the connections failed
     * with multiple devices
     */
    @Test
    public void testNoReconnectOnNoConnect_MultiDevice() {
        // Return a list of bonded devices (just one)
        BluetoothDevice[] bondedDevices = new BluetoothDevice[2];
        bondedDevices[0] = TestUtils.getTestDevice(mAdapter, 0);
        bondedDevices[1] = TestUtils.getTestDevice(mAdapter, 1);
        when(mAdapterService.getBondedDevices()).thenReturn(bondedDevices);

        // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles are
        // auto-connectable.
        when(mHeadsetService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mHeadsetService.getConnectionPolicy(bondedDevices[1])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[1])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);

        // Return an a list with only the second device as connected
        when(mHeadsetService.getConnectedDevices()).thenReturn(
                Collections.singletonList(bondedDevices[1]));
        when(mA2dpService.getConnectedDevices()).thenReturn(
                Collections.singletonList(bondedDevices[1]));

        // Both A2DP and HFP should say this device is not connected, except for the intent
        when(mA2dpService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mHeadsetService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mA2dpService.getConnectionState(bondedDevices[1])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);
        when(mHeadsetService.getConnectionState(bondedDevices[1])).thenReturn(
                BluetoothProfile.STATE_CONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, bondedDevices[0]);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_DISCONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Check that we don't get any calls to reconnect
        verify(mA2dpService, after(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS).never()).connect(
                eq(bondedDevices[0]));
        verify(mHeadsetService, never()).connect(eq(bondedDevices[0]));
    }

    /**
     * Test that we will try to connect to other profiles of a device if it is partially connected
     */
    @Test
    public void testReconnectOnPartialConnect_MultiDevice() {
        // Return a list of bonded devices (just one)
        BluetoothDevice[] bondedDevices = new BluetoothDevice[2];
        bondedDevices[0] = TestUtils.getTestDevice(mAdapter, 0);
        bondedDevices[1] = TestUtils.getTestDevice(mAdapter, 1);
        when(mAdapterService.getBondedDevices()).thenReturn(bondedDevices);

        // Return PRIORITY_AUTO_CONNECT over HFP and A2DP. This would imply that the profiles are
        // auto-connectable.
        when(mHeadsetService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[0])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mHeadsetService.getConnectionPolicy(bondedDevices[1])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        when(mA2dpService.getConnectionPolicy(bondedDevices[1])).thenReturn(
                BluetoothProfile.CONNECTION_POLICY_ALLOWED);

        when(mAdapterService.getState()).thenReturn(BluetoothAdapter.STATE_ON);

        // Return an a list with only the second device as connected
        when(mHeadsetService.getConnectedDevices()).thenReturn(
                Collections.singletonList(bondedDevices[1]));
        when(mA2dpService.getConnectedDevices()).thenReturn(Collections.emptyList());

        // Both A2DP and HFP should say this device is not connected, except for the intent
        when(mA2dpService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mHeadsetService.getConnectionState(bondedDevices[0])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);
        when(mA2dpService.getConnectionState(bondedDevices[1])).thenReturn(
                BluetoothProfile.STATE_DISCONNECTED);

        // We send a connection successful for one profile since the re-connect *only* works if we
        // have already connected successfully over one of the profiles
        updateProfileConnectionStateHelper(bondedDevices[1], BluetoothProfile.HEADSET,
                BluetoothProfile.STATE_CONNECTED, BluetoothProfile.STATE_DISCONNECTED);

        // Check that we don't get any calls to reconnect
        verify(mA2dpService, timeout(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS)).connect(
                eq(bondedDevices[1]));
    }

    /**
     * Test that a device with no supported uuids is initialized properly and does not crash the
     * stack
     */
    @Test
    public void testNoSupportedUuids() {
        // Mock the HeadsetService to return undefined priority
        BluetoothDevice device = TestUtils.getTestDevice(mAdapter, 0);
        when(mHeadsetService.getConnectionPolicy(device))
                .thenReturn(BluetoothProfile.CONNECTION_POLICY_UNKNOWN);

        // Mock the A2DP service to return undefined priority
        when(mA2dpService.getConnectionPolicy(device))
                .thenReturn(BluetoothProfile.CONNECTION_POLICY_UNKNOWN);

        // Inject an event for UUIDs updated for a remote device with only HFP enabled
        Intent intent = new Intent(BluetoothDevice.ACTION_UUID);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);

        // Put no UUIDs
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);

        // Check that we do not crash and not call any setPriority methods
        verify(mHeadsetService,
                after(CONNECT_OTHER_PROFILES_TIMEOUT_WAIT_MILLIS).never())
                .setConnectionPolicy(eq(device), eq(BluetoothProfile.CONNECTION_POLICY_ALLOWED));
        verify(mA2dpService, never())
                .setConnectionPolicy(eq(device), eq(BluetoothProfile.CONNECTION_POLICY_ALLOWED));
    }

    private void updateProfileConnectionStateHelper(BluetoothDevice device, int profileId,
            int nextState, int prevState) {
        Intent intent;
        switch (profileId) {
            case BluetoothProfile.A2DP:
                when(mA2dpService.getConnectionState(device)).thenReturn(nextState);
                intent = new Intent(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
                break;
            case BluetoothProfile.HEADSET:
                when(mHeadsetService.getConnectionState(device)).thenReturn(nextState);
                intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
                break;
            default:
                intent = new Intent(BluetoothAdapter.ACTION_CONNECTION_STATE_CHANGED);
                break;
        }
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, nextState);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mPhonePolicy.getBroadcastReceiver().onReceive(null /* context */, intent);
    }
}
