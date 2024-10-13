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
package com.android.bluetooth.btservice;

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.os.HandlerThread;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.UserHandle;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BondStateMachineTest {
    private static final int TEST_BOND_REASON = 0;
    private static final byte[] TEST_BT_ADDR_BYTES = {00, 11, 22, 33, 44, 55};
    private static final byte[] TEST_BT_ADDR_BYTES_2 = {00, 11, 22, 33, 44, 66};
    private static final ParcelUuid[] TEST_UUIDS =
            {ParcelUuid.fromString("0000111E-0000-1000-8000-00805F9B34FB")};

    private static final int BOND_NONE = BluetoothDevice.BOND_NONE;
    private static final int BOND_BONDING = BluetoothDevice.BOND_BONDING;
    private static final int BOND_BONDED = BluetoothDevice.BOND_BONDED;

    private AdapterProperties mAdapterProperties;
    private BluetoothDevice mDevice;
    private Context mTargetContext;
    private RemoteDevices mRemoteDevices;
    private BondStateMachine mBondStateMachine;
    private HandlerThread mHandlerThread;
    private RemoteDevices.DeviceProperties mDeviceProperties;
    private int mVerifyCount = 0;

    @Mock private AdapterService mAdapterService;

    @Before
    public void setUp() throws Exception {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAdapterService);
        mHandlerThread = new HandlerThread("BondStateMachineTestHandlerThread");
        mHandlerThread.start();

        mRemoteDevices = new RemoteDevices(mAdapterService, mHandlerThread.getLooper());
        mRemoteDevices.reset();
        when(mAdapterService.getResources()).thenReturn(
                mTargetContext.getResources());
        mAdapterProperties = new AdapterProperties(mAdapterService);
        mAdapterProperties.init(mRemoteDevices);
        PowerManager powerManager = (PowerManager) mTargetContext.getSystemService(Context.POWER_SERVICE);
        mBondStateMachine = BondStateMachine.make(powerManager, mAdapterService, mAdapterProperties,
                mRemoteDevices);
    }

    @After
    public void tearDown() throws Exception {
        mHandlerThread.quit();
        TestUtils.clearAdapterService(mAdapterService);
    }

    @Test
    public void testCreateBondAfterRemoveBond() {
        // Set up two devices already bonded.
        mRemoteDevices.reset();
        RemoteDevices.DeviceProperties deviceProperties1, deviceProperties2;
        deviceProperties1 = mRemoteDevices.addDeviceProperties(TEST_BT_ADDR_BYTES);
        deviceProperties2 = mRemoteDevices.addDeviceProperties(TEST_BT_ADDR_BYTES_2);
        BluetoothDevice device1, device2;
        device1 = mRemoteDevices.getDevice(TEST_BT_ADDR_BYTES);
        device2 = mRemoteDevices.getDevice(TEST_BT_ADDR_BYTES_2);
        deviceProperties1.mBondState = BOND_BONDED;
        deviceProperties2.mBondState = BOND_BONDED;

        doReturn(true).when(mAdapterService).removeBondNative(any(byte[].class));
        doReturn(true).when(mAdapterService).createBondNative(any(byte[].class), anyInt());

        // The removeBond() request for a bonded device should invoke the removeBondNative() call.
        Message removeBondMsg1 = mBondStateMachine.obtainMessage(BondStateMachine.REMOVE_BOND);
        removeBondMsg1.obj = device1;
        mBondStateMachine.sendMessage(removeBondMsg1);
        TestUtils.waitForLooperToFinishScheduledTask(mBondStateMachine.getHandler().getLooper());
        Message removeBondMsg2 = mBondStateMachine.obtainMessage(BondStateMachine.REMOVE_BOND);
        removeBondMsg2.obj = device2;
        mBondStateMachine.sendMessage(removeBondMsg2);
        TestUtils.waitForLooperToFinishScheduledTask(mBondStateMachine.getHandler().getLooper());

        verify(mAdapterService, times(1)).removeBondNative(eq(TEST_BT_ADDR_BYTES));
        verify(mAdapterService, times(1)).removeBondNative(eq(TEST_BT_ADDR_BYTES_2));

        mBondStateMachine.bondStateChangeCallback(AbstractionLayer.BT_STATUS_SUCCESS,
                TEST_BT_ADDR_BYTES, BOND_NONE);
        TestUtils.waitForLooperToFinishScheduledTask(mBondStateMachine.getHandler().getLooper());
        mBondStateMachine.bondStateChangeCallback(AbstractionLayer.BT_STATUS_SUCCESS,
                TEST_BT_ADDR_BYTES_2, BOND_NONE);
        TestUtils.waitForLooperToFinishScheduledTask(mBondStateMachine.getHandler().getLooper());

        // Try to pair these two devices again, createBondNative() should be invoked.
        Message createBondMsg1 = mBondStateMachine.obtainMessage(BondStateMachine.CREATE_BOND);
        createBondMsg1.obj = device1;
        mBondStateMachine.sendMessage(createBondMsg1);
        Message createBondMsg2 = mBondStateMachine.obtainMessage(BondStateMachine.CREATE_BOND);
        createBondMsg2.obj = device2;
        mBondStateMachine.sendMessage(createBondMsg2);
        TestUtils.waitForLooperToFinishScheduledTask(mBondStateMachine.getHandler().getLooper());

        verify(mAdapterService, times(1)).createBondNative(eq(TEST_BT_ADDR_BYTES), anyInt());
        verify(mAdapterService, times(1)).createBondNative(eq(TEST_BT_ADDR_BYTES_2), anyInt());
    }

    @Test
    public void testSendIntent() {
        int badBondState = 42;
        mVerifyCount = 0;

        // Reset mRemoteDevices for the test.
        mRemoteDevices.reset();
        mDeviceProperties = mRemoteDevices.addDeviceProperties(TEST_BT_ADDR_BYTES);
        mDevice = mDeviceProperties.getDevice();
        Assert.assertNotNull(mDevice);

        /* Classic / Dualmode test cases*/
        // Uuid not available, mPendingBondedDevice is empty.
        testSendIntentNoPendingDevice(BOND_NONE, BOND_NONE, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_NONE, BOND_BONDING, BOND_BONDING,
                true, BOND_NONE, BOND_BONDING);
        testSendIntentNoPendingDevice(BOND_NONE, BOND_BONDED, BOND_BONDED,
                true, BOND_NONE, BOND_BONDING);
        testSendIntentNoPendingDevice(BOND_NONE, badBondState, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDING, BOND_NONE, BOND_NONE,
                true, BOND_BONDING, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDING, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDING, BOND_BONDED, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDING, badBondState, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDED, BOND_NONE, BOND_NONE,
                true, BOND_BONDED, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDED, BOND_BONDING, BOND_BONDING,
                true, BOND_BONDED, BOND_BONDING);
        testSendIntentNoPendingDevice(BOND_BONDED, BOND_BONDED, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDevice(BOND_BONDED, badBondState, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);

        // Uuid not available, mPendingBondedDevice contains a remote device.
        testSendIntentPendingDevice(BOND_NONE, BOND_NONE, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_NONE, BOND_BONDING, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_NONE, BOND_BONDED, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_NONE, badBondState, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDING, BOND_NONE, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDING, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDING, BOND_BONDED, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDING, badBondState, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDED, BOND_NONE, BOND_NONE,
                true, BOND_BONDING, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDED, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDED, BOND_BONDED, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDevice(BOND_BONDED, badBondState, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);

        // Uuid available, mPendingBondedDevice is empty.
        testSendIntentNoPendingDeviceWithUuid(BOND_NONE, BOND_NONE, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_NONE, BOND_BONDING, BOND_BONDING,
                true, BOND_NONE, BOND_BONDING);
        testSendIntentNoPendingDeviceWithUuid(BOND_NONE, BOND_BONDED, BOND_BONDED,
                true, BOND_NONE, BOND_BONDED);
        testSendIntentNoPendingDeviceWithUuid(BOND_NONE, badBondState, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDING, BOND_NONE, BOND_NONE,
                true, BOND_BONDING, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDING, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDING, BOND_BONDED, BOND_BONDED,
                true, BOND_BONDING, BOND_BONDED);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDING, badBondState, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDED, BOND_NONE, BOND_NONE,
                true, BOND_BONDED, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDED, BOND_BONDING, BOND_BONDING,
                true, BOND_BONDED, BOND_BONDING);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDED, BOND_BONDED, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);
        testSendIntentNoPendingDeviceWithUuid(BOND_BONDED, badBondState, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);

        // Uuid available, mPendingBondedDevice contains a remote device.
        testSendIntentPendingDeviceWithUuid(BOND_NONE, BOND_NONE, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_NONE, BOND_BONDING, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_NONE, BOND_BONDED, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_NONE, badBondState, BOND_NONE,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDING, BOND_NONE, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDING, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDING, BOND_BONDED, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDING, badBondState, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDED, BOND_NONE, BOND_NONE,
                true, BOND_BONDING, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDED, BOND_BONDING, BOND_BONDING,
                false, BOND_NONE, BOND_NONE);
        testSendIntentPendingDeviceWithUuid(BOND_BONDED, BOND_BONDED, BOND_BONDED,
                true, BOND_BONDING, BOND_BONDED);
        testSendIntentPendingDeviceWithUuid(BOND_BONDED, badBondState, BOND_BONDED,
                false, BOND_NONE, BOND_NONE);

        /* Low energy test cases */
        testSendIntentBle(BOND_NONE, BOND_NONE, BOND_NONE);
        testSendIntentBle(BOND_NONE, BOND_BONDING, BOND_BONDING);
        testSendIntentBle(BOND_NONE, BOND_BONDED, BOND_BONDED);
        testSendIntentBle(BOND_BONDING, BOND_NONE, BOND_NONE);
        testSendIntentBle(BOND_BONDING, BOND_BONDING, BOND_BONDING);
        testSendIntentBle(BOND_BONDING, BOND_BONDED, BOND_BONDED);
        testSendIntentBle(BOND_BONDED, BOND_NONE, BOND_NONE);
        testSendIntentBle(BOND_BONDED, BOND_BONDING, BOND_BONDING);
        testSendIntentBle(BOND_BONDED, BOND_BONDED, BOND_BONDED);
    }

    private void testSendIntentCase(int oldState, int newState, int expectedNewState,
            boolean shouldBroadcast, int broadcastOldState, int broadcastNewState) {
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);

        // Setup old state before start test.
        mDeviceProperties.mBondState = oldState;

        try {
            mBondStateMachine.sendIntent(mDevice, newState, TEST_BOND_REASON);
        } catch (IllegalArgumentException e) {
            // Do nothing.
        }
        Assert.assertEquals(expectedNewState, mDeviceProperties.getBondState());

        // Check for bond state Intent status.
        if (shouldBroadcast) {
            verify(mAdapterService, times(++mVerifyCount)).sendBroadcastAsUser(
                    intentArgument.capture(), eq(UserHandle.ALL),
                    eq(AdapterService.BLUETOOTH_PERM));
            verifyBondStateChangeIntent(broadcastOldState, broadcastNewState,
                    intentArgument.getValue());
        } else {
            verify(mAdapterService, times(mVerifyCount)).sendBroadcastAsUser(any(Intent.class),
                    any(UserHandle.class), anyString());
        }
    }

    private void testSendIntentNoPendingDeviceWithUuid(int oldState, int newState,
            int expectedNewState, boolean shouldBroadcast, int broadcastOldState,
            int broadcastNewState) {
        // Add dummy UUID for the device.
        mDeviceProperties.mUuids = TEST_UUIDS;
        testSendIntentNoPendingDevice(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);
    }

    private void testSendIntentPendingDeviceWithUuid(int oldState, int newState,
            int expectedNewState, boolean shouldBroadcast, int broadcastOldState,
            int broadcastNewState) {
        // Add dummy UUID for the device.
        mDeviceProperties.mUuids = TEST_UUIDS;
        testSendIntentPendingDevice(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);
    }

    private void testSendIntentPendingDevice(int oldState, int newState, int expectedNewState,
            boolean shouldBroadcast, int broadcastOldState, int broadcastNewState) {
        // Test for classic remote device.
        mDeviceProperties.mDeviceType = BluetoothDevice.DEVICE_TYPE_CLASSIC;
        mBondStateMachine.mPendingBondedDevices.clear();
        mBondStateMachine.mPendingBondedDevices.add(mDevice);
        testSendIntentCase(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);

        // Test for dual-mode remote device.
        mDeviceProperties.mDeviceType = BluetoothDevice.DEVICE_TYPE_DUAL;
        mBondStateMachine.mPendingBondedDevices.clear();
        mBondStateMachine.mPendingBondedDevices.add(mDevice);
        testSendIntentCase(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);
    }

    private void testSendIntentNoPendingDevice(int oldState, int newState, int expectedNewState,
            boolean shouldBroadcast, int broadcastOldState, int broadcastNewState) {
        // Test for classic remote device.
        mDeviceProperties.mDeviceType = BluetoothDevice.DEVICE_TYPE_CLASSIC;
        mBondStateMachine.mPendingBondedDevices.clear();
        testSendIntentCase(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);

        // Test for dual-mode remote device.
        mDeviceProperties.mDeviceType = BluetoothDevice.DEVICE_TYPE_DUAL;
        mBondStateMachine.mPendingBondedDevices.clear();
        testSendIntentCase(oldState, newState, expectedNewState, shouldBroadcast,
                broadcastOldState, broadcastNewState);
    }

    private void testSendIntentBle(int oldState, int newState, int expectedNewState) {
        // Test for low energy remote device.
        mDeviceProperties.mDeviceType = BluetoothDevice.DEVICE_TYPE_LE;
        mBondStateMachine.mPendingBondedDevices.clear();
        testSendIntentCase(oldState, newState, newState, (oldState != newState),
                oldState, newState);
    }

    private void verifyBondStateChangeIntent(int oldState, int newState, Intent intent) {
        Assert.assertNotNull(intent);
        Assert.assertEquals(BluetoothDevice.ACTION_BOND_STATE_CHANGED, intent.getAction());
        Assert.assertEquals(mDevice, intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE));
        Assert.assertEquals(newState, intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1));
        Assert.assertEquals(oldState, intent.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE,
                                                          -1));
        if (newState == BOND_NONE) {
            Assert.assertEquals(TEST_BOND_REASON, intent.getIntExtra(BluetoothDevice.EXTRA_REASON,
                                                              -1));
        } else {
            Assert.assertEquals(-1, intent.getIntExtra(BluetoothDevice.EXTRA_REASON, -1));
        }
    }
}
