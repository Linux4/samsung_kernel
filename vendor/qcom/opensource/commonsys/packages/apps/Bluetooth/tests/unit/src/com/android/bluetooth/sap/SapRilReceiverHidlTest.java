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

import static com.android.bluetooth.sap.SapMessage.CON_STATUS_OK;
import static com.android.bluetooth.sap.SapMessage.DISC_GRACEFULL;
import static com.android.bluetooth.sap.SapMessage.ID_CONNECT_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_DISCONNECT_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_OFF_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_OFF_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_ON_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_ON_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_RESET_SIM_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_RIL_UNKNOWN;
import static com.android.bluetooth.sap.SapMessage.ID_RIL_UNSOL_DISCONNECT_IND;
import static com.android.bluetooth.sap.SapMessage.ID_SET_TRANSPORT_PROTOCOL_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_STATUS_IND;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_APDU_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_ATR_RESP;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_CARD_READER_STATUS_RESP;
import static com.android.bluetooth.sap.SapMessage.RESULT_OK;
import static com.android.bluetooth.sap.SapMessage.STATUS_CARD_INSERTED;
import static com.android.bluetooth.sap.SapServer.ISAP_GET_SERVICE_DELAY_MILLIS;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RFC_REPLY;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RIL_CONNECT;
import static com.android.bluetooth.sap.SapServer.SAP_MSG_RIL_IND;
import static com.android.bluetooth.sap.SapServer.SAP_PROXY_DEAD;
import static com.android.bluetooth.sap.SapServer.SAP_RIL_SOCK_CLOSED;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.hardware.radio.V1_0.ISap;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;

import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.util.ArrayList;
import java.util.Arrays;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class SapRilReceiverHidlTest {

    private static final long TIMEOUT_MS = 1_000;

    private HandlerThread mHandlerThread;
    private Handler mServerMsgHandler;

    @Spy
    private TestHandlerCallback mCallback = new TestHandlerCallback();

    @Mock
    private Handler mServiceHandler;

    @Mock
    private ISap mSapProxy;

    private SapRilReceiverHidl mReceiver;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mHandlerThread = new HandlerThread("SapRilReceiverTest");
        mHandlerThread.start();

        mServerMsgHandler = new Handler(mHandlerThread.getLooper(), mCallback);
        mReceiver = new SapRilReceiverHidl(mServerMsgHandler, mServiceHandler);
        mReceiver.mSapProxy = mSapProxy;
        mServerMsgHandler.removeMessages(SAP_PROXY_DEAD);
    }

    @After
    public void tearDown() {
        mHandlerThread.quit();
    }

    @Test
    public void getSapProxyLock() {
        assertThat(mReceiver.getSapProxyLock()).isNotNull();
    }

    @Test
    public void resetSapProxy() throws Exception {
        mReceiver.resetSapProxy();

        assertThat(mReceiver.mSapProxy).isNull();
        verify(mSapProxy).unlinkToDeath(any());
    }

    @Test
    public void notifyShutdown() throws Exception {
        mReceiver.notifyShutdown();

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_RIL_SOCK_CLOSED), any());
    }

    @Test
    public void sendRilConnectMessage() throws Exception {
        mReceiver.sendRilConnectMessage();

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RIL_CONNECT), any());
    }

    @Test
    public void serviceDied() throws Exception {
        long cookie = 1;
        mReceiver.mSapProxyDeathRecipient.serviceDied(cookie);

        verify(mCallback, timeout(ISAP_GET_SERVICE_DELAY_MILLIS + TIMEOUT_MS))
                .receiveMessage(eq(SAP_PROXY_DEAD), argThat(
                        arg -> (arg instanceof Long) && ((Long) arg == cookie)
                ));
    }

    @Test
    public void callback_connectResponse() throws Exception {
        int token = 1;
        int sapConnectRsp = CON_STATUS_OK;
        int maxMsgSize = 512;
        mReceiver.mSapCallback.connectResponse(token, sapConnectRsp, maxMsgSize);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_CONNECT_RESP
                                && sapMsg.getConnectionStatus() == sapConnectRsp;
                    }
                }
        ));
    }

    @Test
    public void callback_disconnectResponse() throws Exception {
        int token = 1;
        mReceiver.mSapCallback.disconnectResponse(token);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_DISCONNECT_RESP;
                    }
                }
        ));
    }

    @Test
    public void callback_disconnectIndication() throws Exception {
        int token = 1;
        int disconnectType = DISC_GRACEFULL;
        mReceiver.mSapCallback.disconnectIndication(token, disconnectType);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RIL_IND), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_RIL_UNSOL_DISCONNECT_IND
                                && sapMsg.getDisconnectionType() == disconnectType;
                    }
                }
        ));
    }

    @Test
    public void callback_apduResponse() throws Exception {
        int token = 1;
        int resultCode = RESULT_OK;
        byte[] apduRsp = new byte[]{0x03, 0x04};
        ArrayList<Byte> apduRspList = new ArrayList<>();
        for (byte b : apduRsp) {
            apduRspList.add(b);
        }

        mReceiver.mSapCallback.apduResponse(token, resultCode, apduRspList);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_TRANSFER_APDU_RESP
                                && sapMsg.getResultCode() == resultCode
                                && Arrays.equals(sapMsg.getApduResp(), apduRsp);
                    }
                }
        ));
    }

    @Test
    public void callback_transferAtrResponse() throws Exception {
        int token = 1;
        int resultCode = RESULT_OK;
        byte[] atr = new byte[]{0x03, 0x04};
        ArrayList<Byte> atrList = new ArrayList<>();
        for (byte b : atr) {
            atrList.add(b);
        }

        mReceiver.mSapCallback.transferAtrResponse(token, resultCode, atrList);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_TRANSFER_ATR_RESP
                                && sapMsg.getResultCode() == resultCode
                                && Arrays.equals(sapMsg.getAtr(), atr);
                    }
                }
        ));
    }

    @Test
    public void callback_powerResponse_powerOff() throws Exception {
        int token = 1;
        int reqType = ID_POWER_SIM_OFF_REQ;
        int resultCode = RESULT_OK;
        SapMessage.sOngoingRequests.clear();
        SapMessage.sOngoingRequests.put(token, reqType);

        mReceiver.mSapCallback.powerResponse(token, resultCode);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_POWER_SIM_OFF_RESP
                                && sapMsg.getResultCode() == resultCode;
                    }
                }
        ));
    }

    @Test
    public void callback_powerResponse_powerOn() throws Exception {
        int token = 1;
        int reqType = ID_POWER_SIM_ON_REQ;
        int resultCode = RESULT_OK;
        SapMessage.sOngoingRequests.clear();
        SapMessage.sOngoingRequests.put(token, reqType);

        mReceiver.mSapCallback.powerResponse(token, resultCode);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_POWER_SIM_ON_RESP
                                && sapMsg.getResultCode() == resultCode;
                    }
                }
        ));
    }

    @Test
    public void callback_resetSimResponse() throws Exception {
        int token = 1;
        int resultCode = RESULT_OK;

        mReceiver.mSapCallback.resetSimResponse(token, resultCode);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_RESET_SIM_RESP
                                && sapMsg.getResultCode() == resultCode;
                    }
                }
        ));
    }

    @Test
    public void callback_statusIndication() throws Exception {
        int token = 1;
        int statusChange = 2;

        mReceiver.mSapCallback.statusIndication(token, statusChange);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_STATUS_IND
                                && sapMsg.getStatusChange() == statusChange;
                    }
                }
        ));
    }

    @Test
    public void callback_transferCardReaderStatusResponse() throws Exception {
        int token = 1;
        int resultCode = RESULT_OK;
        int cardReaderStatus = STATUS_CARD_INSERTED;

        mReceiver.mSapCallback.transferCardReaderStatusResponse(
                token, resultCode, cardReaderStatus);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_TRANSFER_CARD_READER_STATUS_RESP
                                && sapMsg.getResultCode() == resultCode;
                    }
                }
        ));
    }

    @Test
    public void callback_errorResponse() throws Exception {
        int token = 1;

        mReceiver.mSapCallback.errorResponse(token);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RIL_IND), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_RIL_UNKNOWN;
                    }
                }
        ));
    }

    @Test
    public void callback_transferProtocolResponse() throws Exception {
        int token = 1;
        int resultCode = RESULT_OK;

        mReceiver.mSapCallback.transferProtocolResponse(token, resultCode);

        verify(mCallback, timeout(TIMEOUT_MS)).receiveMessage(eq(SAP_MSG_RFC_REPLY), argThat(
                new ArgumentMatcher<Object>() {
                    @Override
                    public boolean matches(Object arg) {
                        if (!(arg instanceof SapMessage)) {
                            return false;
                        }
                        SapMessage sapMsg = (SapMessage) arg;
                        return sapMsg.getMsgType() == ID_SET_TRANSPORT_PROTOCOL_RESP
                                && sapMsg.getResultCode() == resultCode;
                    }
                }
        ));
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
