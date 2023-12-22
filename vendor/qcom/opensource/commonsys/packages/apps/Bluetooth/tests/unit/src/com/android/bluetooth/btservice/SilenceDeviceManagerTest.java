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

import static org.mockito.Mockito.*;

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.TestUtils;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.hfp.HeadsetService;

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
public class SilenceDeviceManagerTest {
    private BluetoothAdapter mAdapter;
    private Context mContext;
    private BluetoothDevice mTestDevice;
    private SilenceDeviceManager mSilenceDeviceManager;
    private HandlerThread mHandlerThread;
    private Looper mLooper;
    private static final String TEST_BT_ADDR = "11:22:33:44:55:66";
    private int mVerifyCount = 0;

    @Mock private AdapterService mAdapterService;
    @Mock private ServiceFactory mServiceFactory;
    @Mock private A2dpService mA2dpService;
    @Mock private HeadsetService mHeadsetService;


    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();

        // Set up mocks and test assets
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAdapterService);
        when(mServiceFactory.getA2dpService()).thenReturn(mA2dpService);
        when(mServiceFactory.getHeadsetService()).thenReturn(mHeadsetService);

        // Get devices for testing
        mTestDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(TEST_BT_ADDR);

        mHandlerThread = new HandlerThread("SilenceManagerTestHandlerThread");
        mHandlerThread.start();
        mLooper = mHandlerThread.getLooper();
        mSilenceDeviceManager = new SilenceDeviceManager(mAdapterService, mServiceFactory,
                mLooper);
        mSilenceDeviceManager.start();
    }

    @After
    public void tearDown() throws Exception {
        mSilenceDeviceManager.cleanup();
        mHandlerThread.quit();
        TestUtils.clearAdapterService(mAdapterService);
    }

    @Test
    public void testSetGetDeviceSilence() {
        testSetGetDeviceSilenceConnectedCase(false, true);
        testSetGetDeviceSilenceConnectedCase(false, false);
        testSetGetDeviceSilenceConnectedCase(true, true);
        testSetGetDeviceSilenceConnectedCase(true, false);

        testSetGetDeviceSilenceDisconnectedCase(false);
        testSetGetDeviceSilenceDisconnectedCase(true);
    }

    void testSetGetDeviceSilenceConnectedCase(boolean wasSilenced, boolean enableSilence) {
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        doReturn(true).when(mA2dpService).setSilenceMode(mTestDevice, enableSilence);
        doReturn(true).when(mHeadsetService).setSilenceMode(mTestDevice, enableSilence);

        // Send A2DP/HFP connected intent
        a2dpConnected(mTestDevice);
        headsetConnected(mTestDevice);

        // Set pre-state for mSilenceDeviceManager
        if (wasSilenced) {
            Assert.assertTrue(mSilenceDeviceManager.setSilenceMode(mTestDevice, true));
            TestUtils.waitForLooperToFinishScheduledTask(mLooper);
            verify(mAdapterService, times(++mVerifyCount)).sendBroadcastAsUser(
                    intentArgument.capture(), eq(UserHandle.ALL),
                    eq(AdapterService.BLUETOOTH_PERM));
        }

        // Set silence state and check whether state changed successfully
        Assert.assertTrue(mSilenceDeviceManager.setSilenceMode(mTestDevice, enableSilence));
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
        Assert.assertEquals(enableSilence, mSilenceDeviceManager.getSilenceMode(mTestDevice));

        // Check for silence state changed intent
        if (wasSilenced != enableSilence) {
            verify(mAdapterService, times(++mVerifyCount)).sendBroadcastAsUser(
                    intentArgument.capture(), eq(UserHandle.ALL),
                    eq(AdapterService.BLUETOOTH_PERM));
            verifySilenceStateIntent(intentArgument.getValue());
        }

        // Remove test devices
        a2dpDisconnected(mTestDevice);
        headsetDisconnected(mTestDevice);

        Assert.assertFalse(mSilenceDeviceManager.getSilenceMode(mTestDevice));
        if (enableSilence) {
            // If the silence mode is enabled, it should be automatically disabled
            // after device is disconnected.
            verify(mAdapterService, times(++mVerifyCount)).sendBroadcastAsUser(
                    intentArgument.capture(), eq(UserHandle.ALL),
                    eq(AdapterService.BLUETOOTH_PERM));
        }
    }

    void testSetGetDeviceSilenceDisconnectedCase(boolean enableSilence) {
        ArgumentCaptor<Intent> intentArgument = ArgumentCaptor.forClass(Intent.class);
        // Set silence mode and it should stay disabled
        Assert.assertTrue(mSilenceDeviceManager.setSilenceMode(mTestDevice, enableSilence));
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
        Assert.assertFalse(mSilenceDeviceManager.getSilenceMode(mTestDevice));

        // Should be no intent been broadcasted
        verify(mAdapterService, times(mVerifyCount)).sendBroadcastAsUser(
                intentArgument.capture(), eq(UserHandle.ALL),
                eq(AdapterService.BLUETOOTH_PERM));
    }

    void verifySilenceStateIntent(Intent intent) {
        Assert.assertEquals(BluetoothDevice.ACTION_SILENCE_MODE_CHANGED, intent.getAction());
        Assert.assertEquals(mTestDevice, intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE));
    }

    /**
     * Helper to indicate A2dp connected for a device.
     */
    private void a2dpConnected(BluetoothDevice device) {
        Intent intent = new Intent(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_DISCONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        mSilenceDeviceManager.getBroadcastReceiver().onReceive(mContext, intent);
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
    }

    /**
     * Helper to indicate A2dp disconnected for a device.
     */
    private void a2dpDisconnected(BluetoothDevice device) {
        Intent intent = new Intent(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_CONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_DISCONNECTED);
        mSilenceDeviceManager.getBroadcastReceiver().onReceive(mContext, intent);
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
    }

    /**
     * Helper to indicate Headset connected for a device.
     */
    private void headsetConnected(BluetoothDevice device) {
        Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_DISCONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_CONNECTED);
        mSilenceDeviceManager.getBroadcastReceiver().onReceive(mContext, intent);
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
    }

    /**
     * Helper to indicate Headset disconnected for a device.
     */
    private void headsetDisconnected(BluetoothDevice device) {
        Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, BluetoothProfile.STATE_CONNECTED);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, BluetoothProfile.STATE_DISCONNECTED);
        mSilenceDeviceManager.getBroadcastReceiver().onReceive(mContext, intent);
        TestUtils.waitForLooperToFinishScheduledTask(mLooper);
    }
}

