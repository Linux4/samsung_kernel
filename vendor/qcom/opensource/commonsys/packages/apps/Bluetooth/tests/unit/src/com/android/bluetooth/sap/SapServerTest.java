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

package com.android.bluetooth.sap;

import static com.android.bluetooth.sap.SapMessage.CON_STATUS_ERROR_CONNECTION;
import static com.android.bluetooth.sap.SapMessage.CON_STATUS_OK;
import static com.android.bluetooth.sap.SapMessage.CON_STATUS_OK_ONGOING_CALL;
import static com.android.bluetooth.sap.SapMessage.DISC_GRACEFULL;
import static com.android.bluetooth.sap.SapMessage.ID_CONNECT_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_CONNECT_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_DISCONNECT_IND;
import static com.android.bluetooth.sap.SapMessage.ID_DISCONNECT_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_ERROR_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_RIL_UNSOL_DISCONNECT_IND;
import static com.android.bluetooth.sap.SapMessage.ID_STATUS_IND;
import static com.android.bluetooth.sap.SapMessage.TEST_MODE_ENABLE;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RFC_REPLY;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RIL_CONNECT;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RIL_IND;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RIL_REQ;
import static com.android.bluetooth.sap.SapServer.SAP_PROXY_DEAD;
import static com.android.bluetooth.sap.SapServer.SAP_RIL_SOCK_CLOSED;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.os.RemoteException;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.atomic.AtomicLong;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SapServerTest {
    private static final long TIMEOUT_MS = 1_000;

    private HandlerThread mHandlerThread;
    private Handler mHandler;

    @Spy
    private Context mTargetContext =
            new ContextWrapper(InstrumentationRegistry.getInstrumentation().getTargetContext());

    @Spy
    private TestHandlerCallback mCallback = new TestHandlerCallback();

    @Mock
    private InputStream mInputStream;

    @Mock
    private OutputStream mOutputStream;

    private SapServer mSapServer;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mHandlerThread = new HandlerThread("SapServerTest");
        mHandlerThread.start();

        mHandler = new Handler(mHandlerThread.getLooper(), mCallback);
        mSapServer = spy(new SapServer(mHandler, mTargetContext, mInputStream, mOutputStream));
    }

    @After
    public void tearDown() {
        mHandlerThread.quit();
    }

    @Test
    public void setNotification() {
        NotificationManager notificationManager = mock(NotificationManager.class);
        when(mTargetContext.getSystemService(NotificationManager.class))
                .thenReturn(notificationManager);

        ArgumentCaptor<Notification> captor = ArgumentCaptor.forClass(Notification.class);
        int type = DISC_GRACEFULL;
        int flags = PendingIntent.FLAG_CANCEL_CURRENT;
        mSapServer.setNotification(type, flags);

        verify(notificationManager).notify(eq(SapServer.NOTIFICATION_ID), captor.capture());
        Notification notification = captor.getValue();
        assertThat(notification.getChannelId()).isEqualTo(SapServer.SAP_NOTIFICATION_CHANNEL);
    }

    @Test
    public void clearNotification() {
        NotificationManager notificationManager = mock(NotificationManager.class);
        when(mTargetContext.getSystemService(NotificationManager.class))
                .thenReturn(notificationManager);

        mSapServer.clearNotification();

        verify(notificationManager).cancel(SapServer.NOTIFICATION_ID);
    }

    @Test
    public void setTestMode() {
        int testMode = TEST_MODE_ENABLE;
        mSapServer.setTestMode(testMode);

        assertThat(mSapServer.mTestMode).isEqualTo(testMode);
    }

    @Test
    public void onConnectRequest_whenStateIsConnecting_callsSendRilMessage() {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTING);
        SapMessage msg = new SapMessage(ID_STATUS_IND);
        mSapServer.onConnectRequest(msg);

        verify(mSapServer).sendRilMessage(msg);
    }

    @Test
    public void onConnectRequest_whenStateIsConnected_sendsErrorConnectionClientMessage() {
        mSapServer.mSapHandler = mHandler;

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.onConnectRequest(mock(SapMessage.class));

        verify(mSapServer).sendClientMessage(argThat(
                sapMsg -> sapMsg.getMsgType() == ID_CONNECT_RESP
                        && sapMsg.getConnectionStatus() == CON_STATUS_ERROR_CONNECTION));
    }

    @Test
    public void onConnectRequest_whenStateIsCallOngoing_sendsErrorConnectionClientMessage() {
        mSapServer.mSapHandler = mHandler;

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTING_CALL_ONGOING);
        mSapServer.onConnectRequest(mock(SapMessage.class));

        verify(mSapServer, atLeastOnce()).sendClientMessage(argThat(
                sapMsg -> sapMsg.getMsgType() == ID_CONNECT_RESP
                        && sapMsg.getConnectionStatus() == CON_STATUS_ERROR_CONNECTION));
    }

    @Test
    public void getMessageName() {
        assertThat(SapServer.getMessageName(SAP_MSG_RFC_REPLY)).isEqualTo("SAP_MSG_REPLY");
        assertThat(SapServer.getMessageName(SAP_MSG_RIL_CONNECT)).isEqualTo("SAP_MSG_RIL_CONNECT");
        assertThat(SapServer.getMessageName(SAP_MSG_RIL_REQ)).isEqualTo("SAP_MSG_RIL_REQ");
        assertThat(SapServer.getMessageName(SAP_MSG_RIL_IND)).isEqualTo("SAP_MSG_RIL_IND");
        assertThat(SapServer.getMessageName(-1)).isEqualTo("Unknown message ID");
    }

    @Test
    public void sendReply() throws Exception {
        SapMessage msg = mock(SapMessage.class);
        mSapServer.sendReply(msg);

        verify(msg).write(any(OutputStream.class));
    }

    @Test
    public void sendRilMessage_success() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        when(mockReceiver.isProxyValid()).thenReturn(true);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = mock(SapMessage.class);
        mSapServer.sendRilMessage(msg);

        verify(msg).send(mockReceiver);
    }

    @Test
    public void sendRilMessage_whenSapProxyIsNull_sendsErrorClientMessage() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        when(mockReceiver.isProxyValid()).thenReturn(false);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = mock(SapMessage.class);
        mSapServer.sendRilMessage(msg);

        verify(mSapServer).sendClientMessage(
                argThat(sapMsg -> sapMsg.getMsgType() == ID_ERROR_RESP));
    }

    @Test
    public void sendRilMessage_whenIAEIsThrown_sendsErrorClientMessage() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = mock(SapMessage.class);
        doThrow(new IllegalArgumentException()).when(msg).send(any());
        mSapServer.sendRilMessage(msg);

        verify(mSapServer).sendClientMessage(
                argThat(sapMsg -> sapMsg.getMsgType() == ID_ERROR_RESP));
    }

    @Test
    public void sendRilMessage_whenRemoteExceptionIsThrown_sendsErrorClientMessage()
            throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        when(mockReceiver.isProxyValid()).thenReturn(true);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = mock(SapMessage.class);
        doThrow(new RemoteException()).when(msg).send(any());
        mSapServer.sendRilMessage(msg);

        verify(mSapServer).sendClientMessage(
                argThat(sapMsg -> sapMsg.getMsgType() == ID_ERROR_RESP));
        verify(mockReceiver).notifyShutdown();
        verify(mockReceiver).resetSapProxy();
    }

    @Test
    public void handleRilInd_whenMessageIsNull() {
        try {
            mSapServer.handleRilInd(null);
        } catch (Exception e) {
            assertWithMessage("Exception should not happen.").fail();
        }
    }

    @Test
    public void handleRilInd_whenStateIsConnected_callsSendClientMessage() {
        int disconnectionType = DISC_GRACEFULL;
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_RIL_UNSOL_DISCONNECT_IND);
        when(msg.getDisconnectionType()).thenReturn(disconnectionType);
        mSapServer.mSapHandler = mHandler;

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.handleRilInd(msg);

        verify(mSapServer).sendClientMessage(argThat(
                sapMsg -> sapMsg.getMsgType() == ID_DISCONNECT_IND
                        && sapMsg.getDisconnectionType() == disconnectionType));
    }

    @Test
    public void handleRilInd_whenStateIsDisconnected_callsSendDisconnectInd() {
        int disconnectionType = DISC_GRACEFULL;
        NotificationManager notificationManager = mock(NotificationManager.class);
        when(mTargetContext.getSystemService(NotificationManager.class))
                .thenReturn(notificationManager);
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_RIL_UNSOL_DISCONNECT_IND);
        when(msg.getDisconnectionType()).thenReturn(disconnectionType);
        mSapServer.mSapHandler = mHandler;

        mSapServer.changeState(SapServer.SAP_STATE.DISCONNECTED);
        mSapServer.handleRilInd(msg);

        verify(mSapServer).sendDisconnectInd(disconnectionType);
    }

    @Test
    public void handleRfcommReply_whenMessageIsNull() {
        try {
            mSapServer.changeState(SapServer.SAP_STATE.CONNECTED_BUSY);
            mSapServer.handleRfcommReply(null);
        } catch (Exception e) {
            assertWithMessage("Exception should not happen.").fail();
        }
    }

    @Test
    public void handleRfcommReply_connectRespMsg_whenInCallOngoingState() {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_CONNECT_RESP);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTING_CALL_ONGOING);
        when(msg.getConnectionStatus()).thenReturn(CON_STATUS_OK);
        mSapServer.handleRfcommReply(msg);

        assertThat(mSapServer.mState).isEqualTo(SapServer.SAP_STATE.CONNECTED);
    }

    @Test
    public void handleRfcommReply_connectRespMsg_whenNotInCallOngoingState_okStatus() {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_CONNECT_RESP);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        when(msg.getConnectionStatus()).thenReturn(CON_STATUS_OK);
        mSapServer.handleRfcommReply(msg);

        assertThat(mSapServer.mState).isEqualTo(SapServer.SAP_STATE.CONNECTED);
    }

    @Test
    public void handleRfcommReply_connectRespMsg_whenNotInCallOngoingState_ongoingCallStatus() {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_CONNECT_RESP);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        when(msg.getConnectionStatus()).thenReturn(CON_STATUS_OK_ONGOING_CALL);
        mSapServer.handleRfcommReply(msg);

        assertThat(mSapServer.mState).isEqualTo(SapServer.SAP_STATE.CONNECTING_CALL_ONGOING);
    }

    @Test
    public void handleRfcommReply_connectRespMsg_whenNotInCallOngoingState_errorStatus() {
        AlarmManager alarmManager = mock(AlarmManager.class);
        when(mTargetContext.getSystemService(AlarmManager.class)).thenReturn(alarmManager);
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_CONNECT_RESP);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        when(msg.getConnectionStatus()).thenReturn(CON_STATUS_ERROR_CONNECTION);
        mSapServer.handleRfcommReply(msg);

        verify(mSapServer).startDisconnectTimer(anyInt(), anyInt());
    }

    @Test
    public void handleRfcommReply_disconnectRespMsg_whenInDisconnectingState() {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_DISCONNECT_RESP);

        mSapServer.changeState(SapServer.SAP_STATE.DISCONNECTING);
        mSapServer.handleRfcommReply(msg);

        assertThat(mSapServer.mState).isEqualTo(SapServer.SAP_STATE.DISCONNECTED);
    }

    @Test
    public void handleRfcommReply_disconnectRespMsg_whenInConnectedState_shutDown() {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_DISCONNECT_RESP);

        mSapServer.mIsLocalInitDisconnect = true;
        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.handleRfcommReply(msg);

        verify(mSapServer).shutdown();
    }

    @Test
    public void handleRfcommReply_disconnectRespMsg_whenInConnectedState_startsDisconnectTimer() {
        AlarmManager alarmManager = mock(AlarmManager.class);
        when(mTargetContext.getSystemService(AlarmManager.class)).thenReturn(alarmManager);
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_DISCONNECT_RESP);

        mSapServer.mIsLocalInitDisconnect = false;
        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.handleRfcommReply(msg);

        verify(mSapServer).startDisconnectTimer(anyInt(), anyInt());
    }

    @Test
    public void handleRfcommReply_statusIndMsg_whenInDisonnectingState_doesNotSendMessage()
            throws Exception {
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_STATUS_IND);

        mSapServer.changeState(SapServer.SAP_STATE.DISCONNECTING);
        mSapServer.handleRfcommReply(msg);

        verify(msg, never()).send(any());
    }

    @Test
    public void handleRfcommReply_statusIndMsg_whenInConnectedState_setsNotification() {
        NotificationManager notificationManager = mock(NotificationManager.class);
        when(mTargetContext.getSystemService(NotificationManager.class))
                .thenReturn(notificationManager);
        SapMessage msg = mock(SapMessage.class);
        when(msg.getMsgType()).thenReturn(ID_STATUS_IND);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.handleRfcommReply(msg);

        verify(notificationManager).notify(eq(SapServer.NOTIFICATION_ID), any());
    }

    @Test
    public void startDisconnectTimer_and_stopDisconnectTimer() {
        AlarmManager alarmManager = mock(AlarmManager.class);
        when(mTargetContext.getSystemService(AlarmManager.class)).thenReturn(alarmManager);

        mSapServer.startDisconnectTimer(SapMessage.DISC_FORCED, 1_000);
        verify(alarmManager).set(anyInt(), anyLong(), any(PendingIntent.class));

        mSapServer.stopDisconnectTimer();
        verify(alarmManager).cancel(any(PendingIntent.class));
    }

    @Test
    public void isCallOngoing() {
        TelephonyManager telephonyManager = mock(TelephonyManager.class);
        when(mTargetContext.getSystemService(TelephonyManager.class)).thenReturn(telephonyManager);

        when(telephonyManager.getCallState()).thenReturn(TelephonyManager.CALL_STATE_OFFHOOK);
        assertThat(mSapServer.isCallOngoing()).isTrue();

        when(telephonyManager.getCallState()).thenReturn(TelephonyManager.CALL_STATE_IDLE);
        assertThat(mSapServer.isCallOngoing()).isFalse();
    }

    @Test
    public void sendRilThreadMessage() {
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = new SapMessage(ID_STATUS_IND);
        mSapServer.sendRilThreadMessage(msg);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RIL_REQ), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        return msg == arg;
                    }
                }
        ));
    }

    @Test
    public void sendClientMessage() {
        mSapServer.mSapHandler = mHandler;

        SapMessage msg = new SapMessage(ID_STATUS_IND);
        mSapServer.sendClientMessage(msg);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        return msg == arg;
                    }
                }
        ));
    }

    // TODO: Find a good way to run() method.

    @Test
    public void clearPendingRilResponses_whenInConnectedBusyState_setsClearRilQueueAsTrue() {
        SapMessage msg = mock(SapMessage.class);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED_BUSY);
        mSapServer.clearPendingRilResponses(msg);

        verify(msg).setClearRilQueue(true);
    }

    @Test
    public void handleMessage_forRfcReplyMsg_callsHandleRfcommReply() {
        SapMessage sapMsg = mock(SapMessage.class);
        when(sapMsg.getMsgType()).thenReturn(ID_CONNECT_RESP);
        when(sapMsg.getConnectionStatus()).thenReturn(CON_STATUS_OK);
        mSapServer.changeState(SapServer.SAP_STATE.DISCONNECTED);

        Message message = Message.obtain();
        message.what = SAP_MSG_RFC_REPLY;
        message.obj = sapMsg;

        try {
            mSapServer.handleMessage(message);

            verify(mSapServer).handleRfcommReply(sapMsg);
        } finally {
            message.recycle();
        }
    }

    @Test
    public void handleMessage_forRilConnectMsg_callsSendRilMessage() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;
        mSapServer.setTestMode(TEST_MODE_ENABLE);

        Message message = Message.obtain();
        message.what = SAP_MSG_RIL_CONNECT;

        try {
            mSapServer.handleMessage(message);

            verify(mSapServer).sendRilMessage(
                    argThat(sapMsg -> sapMsg.getMsgType() == ID_CONNECT_REQ));
        } finally {
            message.recycle();
        }
    }

    @Test
    public void handleMessage_forRilReqMsg_callsSendRilMessage() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        Object lock = new Object();
        when(mockReceiver.getSapProxyLock()).thenReturn(lock);
        mSapServer.mRilBtReceiver = mockReceiver;
        mSapServer.mSapHandler = mHandler;

        SapMessage sapMsg = mock(SapMessage.class);
        when(sapMsg.getMsgType()).thenReturn(ID_CONNECT_REQ);

        Message message = Message.obtain();
        message.what = SAP_MSG_RIL_REQ;
        message.obj = sapMsg;

        try {
            mSapServer.handleMessage(message);

            verify(mSapServer).sendRilMessage(sapMsg);
        } finally {
            message.recycle();
        }
    }

    @Test
    public void handleMessage_forRilIndMsg_callsHandleRilInd() throws Exception {
        SapMessage sapMsg = mock(SapMessage.class);
        when(sapMsg.getMsgType()).thenReturn(ID_RIL_UNSOL_DISCONNECT_IND);
        when(sapMsg.getDisconnectionType()).thenReturn(DISC_GRACEFULL);
        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.mSapHandler = mHandler;

        Message message = Message.obtain();
        message.what = SAP_MSG_RIL_IND;
        message.obj = sapMsg;

        try {
            mSapServer.handleMessage(message);

            verify(mSapServer).handleRilInd(sapMsg);
        } finally {
            message.recycle();
        }
    }

    @Test
    public void handleMessage_forRilSocketClosedMsg_startsDisconnectTimer() throws Exception {
        AlarmManager alarmManager = mock(AlarmManager.class);
        when(mTargetContext.getSystemService(AlarmManager.class)).thenReturn(alarmManager);

        Message message = Message.obtain();
        message.what = SAP_RIL_SOCK_CLOSED;

        try {
            mSapServer.handleMessage(message);

            verify(mSapServer).startDisconnectTimer(anyInt(), anyInt());
        } finally {
            message.recycle();
        }
    }

    @Test
    public void handleMessage_forProxyDeadMsg_notifiesShutDown() throws Exception {
        ISapRilReceiver mockReceiver = mock(ISapRilReceiver.class);
        AtomicLong cookie = new AtomicLong(23);
        mSapServer.mRilBtReceiver = mockReceiver;

        Message message = Message.obtain();
        message.what = SAP_PROXY_DEAD;
        message.obj = cookie.get();

        try {
            mSapServer.handleMessage(message);

            verify(mockReceiver).notifyShutdown();
        } finally {
            message.recycle();
        }
    }

    @Test
    public void onReceive_phoneStateChangedAction_whenStateIsCallOngoing_callsOnConnectRequest() {
        mSapServer.mIntentReceiver = mSapServer.new SapServerBroadcastReceiver();
        mSapServer.mSapHandler = mHandler;
        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_IDLE);

        mSapServer.changeState(SapServer.SAP_STATE.CONNECTING_CALL_ONGOING);
        assertThat(mSapServer.mState).isEqualTo(SapServer.SAP_STATE.CONNECTING_CALL_ONGOING);
        mSapServer.mIntentReceiver.onReceive(mTargetContext, intent);

        verify(mSapServer).onConnectRequest(
                argThat(sapMsg -> sapMsg.getMsgType() == ID_CONNECT_REQ));
    }

    @Test
    public void onReceive_SapDisconnectedAction_forDiscRfcommType_callsShutDown() {
        mSapServer.mIntentReceiver = mSapServer.new SapServerBroadcastReceiver();

        int disconnectType = SapMessage.DISC_RFCOMM;
        Intent intent = new Intent(SapServer.SAP_DISCONNECT_ACTION);
        intent.putExtra(SapServer.SAP_DISCONNECT_TYPE_EXTRA, disconnectType);
        mSapServer.mIntentReceiver.onReceive(mTargetContext, intent);

        verify(mSapServer).shutdown();
    }

    @Test
    public void onReceive_SapDisconnectedAction_forNonDiscRfcommType_callsSendDisconnectInd() {
        mSapServer.mIntentReceiver = mSapServer.new SapServerBroadcastReceiver();
        mSapServer.mSapHandler = mHandler;

        int disconnectType = SapMessage.DISC_GRACEFULL;
        Intent intent = new Intent(SapServer.SAP_DISCONNECT_ACTION);
        intent.putExtra(SapServer.SAP_DISCONNECT_TYPE_EXTRA, disconnectType);
        mSapServer.changeState(SapServer.SAP_STATE.CONNECTED);
        mSapServer.mIntentReceiver.onReceive(mTargetContext, intent);

        verify(mSapServer).sendDisconnectInd(disconnectType);
    }

    @Test
    public void onReceive_unknownAction_doesNothing() {
        Intent intent = new Intent("random intent action");

        try {
            mSapServer.mIntentReceiver.onReceive(mTargetContext, intent);
        } catch (Exception e) {
            assertWithMessage("Exception should not happen.").fail();
        }
    }

    public static class TestHandlerCallback implements Handler.Callback {

        @Override
        public boolean handleMessage(Message msg) {
            receiveMessage(msg.what, msg.obj);
            return true;
        }

        public void receiveMessage(int what, Object obj) {
        }
    }
}

