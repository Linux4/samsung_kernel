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
import android.bluetooth.BluetoothMapClient;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.SdpMasRecord;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.btservice.ProfileService;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class MapClientStateMachineTest {
    private static final String TAG = "MapStateMachineTest";

    private static final int ASYNC_CALL_TIMEOUT_MILLIS = 100;

    private BluetoothAdapter mAdapter;
    private MceStateMachine mMceStateMachine = null;
    private BluetoothDevice mTestDevice;
    private Context mTargetContext;

    private Handler mHandler;

    private ArgumentCaptor<Intent> mIntentArgument = ArgumentCaptor.forClass(Intent.class);

    @Mock
    private MapClientService mMockMapClientService;

    @Mock
    private MasClient mMockMasClient;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTargetContext = InstrumentationRegistry.getTargetContext();
        Assume.assumeTrue("Ignore test when MapClientService is not enabled",
                mTargetContext.getResources().getBoolean(R.bool.profile_supported_mapmce));

        doReturn(mTargetContext.getResources()).when(mMockMapClientService).getResources();

        // This line must be called to make sure relevant objects are initialized properly
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        // Get a device for testing
        mTestDevice = mAdapter.getRemoteDevice("00:01:02:03:04:05");

        when(mMockMasClient.makeRequest(any(Request.class))).thenReturn(true);
        mMceStateMachine = new MceStateMachine(mMockMapClientService, mTestDevice, mMockMasClient);
        Assert.assertNotNull(mMceStateMachine);
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        mHandler = new Handler();
    }

    @After
    public void tearDown() {
        if (!mTargetContext.getResources().getBoolean(R.bool.profile_supported_mapmce)) {
            return;
        }
        if (mMceStateMachine != null) {
            mMceStateMachine.doQuit();
        }
    }

    /**
     * Test that default state is STATE_CONNECTING
     */
    @Test
    public void testDefaultConnectingState() {
        Log.i(TAG, "in testDefaultConnectingState");
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING, mMceStateMachine.getState());
    }

    /**
     * Test transition from
     *      STATE_CONNECTING --> (receive MSG_MAS_DISCONNECTED) --> STATE_DISCONNECTED
     */
    @Test
    public void testStateTransitionFromConnectingToDisconnected() {
        Log.i(TAG, "in testStateTransitionFromConnectingToDisconnected");
        setupSdpRecordReceipt();
        Message msg = Message.obtain(mHandler, MceStateMachine.MSG_MAS_DISCONNECTED);
        mMceStateMachine.sendMessage(msg);

        // Wait until the message is processed and a broadcast request is sent to
        // to MapClientService to change
        // state from STATE_CONNECTING to STATE_DISCONNECTED
        verify(mMockMapClientService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(BluetoothProfile.STATE_DISCONNECTED, mMceStateMachine.getState());
    }

    /**
     * Test transition from STATE_CONNECTING --> (receive MSG_MAS_CONNECTED) --> STATE_CONNECTED
     */
    @Test
    public void testStateTransitionFromConnectingToConnected() {
        Log.i(TAG, "in testStateTransitionFromConnectingToConnected");

        setupSdpRecordReceipt();
        Message msg = Message.obtain(mHandler, MceStateMachine.MSG_MAS_CONNECTED);
        mMceStateMachine.sendMessage(msg);

        // Wait until the message is processed and a broadcast request is sent to
        // to MapClientService to change
        // state from STATE_CONNECTING to STATE_CONNECTED
        verify(mMockMapClientService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED, mMceStateMachine.getState());
    }

    /**
     * Test receiving an empty event report
     */
    @Test
    public void testReceiveEmptyEvent() {
        setupSdpRecordReceipt();
        Message msg = Message.obtain(mHandler, MceStateMachine.MSG_MAS_CONNECTED);
        mMceStateMachine.sendMessage(msg);

        // Wait until the message is processed and a broadcast request is sent to
        // to MapClientService to change
        // state from STATE_CONNECTING to STATE_CONNECTED
        verify(mMockMapClientService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED, mMceStateMachine.getState());

        // Send an empty notification event, verify the mMceStateMachine is still connected
        Message notification = Message.obtain(mHandler, MceStateMachine.MSG_NOTIFICATION);
        mMceStateMachine.getCurrentState().processMessage(msg);
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED, mMceStateMachine.getState());
    }

    /**
     * Test set message status
     */
    @Test
    public void testSetMessageStatus() {
        setupSdpRecordReceipt();
        Message msg = Message.obtain(mHandler, MceStateMachine.MSG_MAS_CONNECTED);
        mMceStateMachine.sendMessage(msg);

        // Wait until the message is processed and a broadcast request is sent to
        // to MapClientService to change
        // state from STATE_CONNECTING to STATE_CONNECTED
        verify(mMockMapClientService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(2)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTED, mMceStateMachine.getState());
        Assert.assertTrue(mMceStateMachine.setMessageStatus("123456789AB", BluetoothMapClient.READ));
    }

    private void setupSdpRecordReceipt() {
        // Perform first part of MAP connection logic.
        verify(mMockMapClientService,
                timeout(ASYNC_CALL_TIMEOUT_MILLIS).times(1)).sendBroadcast(
                mIntentArgument.capture(), eq(ProfileService.BLUETOOTH_PERM));
        Assert.assertEquals(BluetoothProfile.STATE_CONNECTING, mMceStateMachine.getState());

        // Setup receipt of SDP record
        SdpMasRecord record = new SdpMasRecord(1, 1, 1, 1, 1, 1, "MasRecord");
        Message msg = Message.obtain(mHandler, MceStateMachine.MSG_MAS_SDP_DONE, record);
        mMceStateMachine.sendMessage(msg);
    }

}
