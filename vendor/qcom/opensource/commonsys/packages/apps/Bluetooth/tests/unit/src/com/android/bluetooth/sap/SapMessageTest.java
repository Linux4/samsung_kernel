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
import static com.android.bluetooth.sap.SapMessage.ID_CONNECT_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_DISCONNECT_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_OFF_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_POWER_SIM_ON_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_RESET_SIM_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_SET_TRANSPORT_PROTOCOL_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_APDU_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_ATR_REQ;
import static com.android.bluetooth.sap.SapMessage.ID_TRANSFER_CARD_READER_STATUS_REQ;
import static com.android.bluetooth.sap.SapMessage.RESULT_OK;
import static com.android.bluetooth.sap.SapMessage.STATUS_CARD_INSERTED;
import static com.android.bluetooth.sap.SapMessage.TEST_MODE_ENABLE;
import static com.android.bluetooth.sap.SapMessage.TRANS_PROTO_T0;
import static com.android.bluetooth.sap.SapMessage.TRANS_PROTO_T1;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.hardware.radio.sap.SapTransferProtocol;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SapMessageTest {

    private SapMessage mMessage;

    @Before
    public void setUp() throws Exception {
        mMessage = new SapMessage(ID_CONNECT_REQ);
    }

    @Test
    public void settersAndGetters() {
        int msgType = ID_CONNECT_REQ;
        int maxMsgSize = 512;
        int connectionStatus = CON_STATUS_OK;
        int resultCode = RESULT_OK;
        int disconnectionType = DISC_GRACEFULL;
        int cardReaderStatus = STATUS_CARD_INSERTED;
        int statusChange = 1;
        int transportProtocol = TRANS_PROTO_T0;
        byte[] apdu = new byte[]{0x01, 0x02};
        byte[] apdu7816 = new byte[]{0x03, 0x04};
        byte[] apduResp = new byte[]{0x05, 0x06};
        byte[] atr = new byte[]{0x07, 0x08};
        boolean sendToRil = true;
        boolean clearRilQueue = true;
        int testMode = TEST_MODE_ENABLE;

        mMessage.setMsgType(msgType);
        mMessage.setMaxMsgSize(maxMsgSize);
        mMessage.setConnectionStatus(connectionStatus);
        mMessage.setResultCode(resultCode);
        mMessage.setDisconnectionType(disconnectionType);
        mMessage.setCardReaderStatus(cardReaderStatus);
        mMessage.setStatusChange(statusChange);
        mMessage.setTransportProtocol(transportProtocol);
        mMessage.setApdu(apdu);
        mMessage.setApdu7816(apdu7816);
        mMessage.setApduResp(apduResp);
        mMessage.setAtr(atr);
        mMessage.setSendToRil(sendToRil);
        mMessage.setClearRilQueue(clearRilQueue);
        mMessage.setTestMode(testMode);

        assertThat(mMessage.getMsgType()).isEqualTo(msgType);
        assertThat(mMessage.getMaxMsgSize()).isEqualTo(maxMsgSize);
        assertThat(mMessage.getConnectionStatus()).isEqualTo(connectionStatus);
        assertThat(mMessage.getResultCode()).isEqualTo(resultCode);
        assertThat(mMessage.getDisconnectionType()).isEqualTo(disconnectionType);
        assertThat(mMessage.getCardReaderStatus()).isEqualTo(cardReaderStatus);
        assertThat(mMessage.getStatusChange()).isEqualTo(statusChange);
        assertThat(mMessage.getTransportProtocol()).isEqualTo(transportProtocol);
        assertThat(mMessage.getApdu()).isEqualTo(apdu);
        assertThat(mMessage.getApdu7816()).isEqualTo(apdu7816);
        assertThat(mMessage.getApduResp()).isEqualTo(apduResp);
        assertThat(mMessage.getAtr()).isEqualTo(atr);
        assertThat(mMessage.getSendToRil()).isEqualTo(sendToRil);
        assertThat(mMessage.getClearRilQueue()).isEqualTo(clearRilQueue);
        assertThat(mMessage.getTestMode()).isEqualTo(testMode);
    }

    @Test
    public void getParamCount() {
        int paramCount = 3;

        mMessage.setMaxMsgSize(512);
        mMessage.setConnectionStatus(CON_STATUS_OK);
        mMessage.setResultCode(RESULT_OK);

        assertThat(mMessage.getParamCount()).isEqualTo(paramCount);
    }

    @Test
    public void getNumPendingRilMessages() {
        SapMessage.sOngoingRequests.put(/*rilSerial=*/10000, ID_CONNECT_REQ);
        assertThat(SapMessage.getNumPendingRilMessages()).isEqualTo(1);

        SapMessage.resetPendingRilMessages();
        assertThat(SapMessage.getNumPendingRilMessages()).isEqualTo(0);
    }

    @Test
    public void writeAndRead() throws Exception {
        int msgType = ID_CONNECT_REQ;
        int maxMsgSize = 512;
        int connectionStatus = CON_STATUS_OK;
        int resultCode = RESULT_OK;
        int disconnectionType = DISC_GRACEFULL;
        int cardReaderStatus = STATUS_CARD_INSERTED;
        int statusChange = 1;
        int transportProtocol = TRANS_PROTO_T0;
        byte[] apdu = new byte[]{0x01, 0x02};
        byte[] apdu7816 = new byte[]{0x03, 0x04};
        byte[] apduResp = new byte[]{0x05, 0x06};
        byte[] atr = new byte[]{0x07, 0x08};

        mMessage.setMsgType(msgType);
        mMessage.setMaxMsgSize(maxMsgSize);
        mMessage.setConnectionStatus(connectionStatus);
        mMessage.setResultCode(resultCode);
        mMessage.setDisconnectionType(disconnectionType);
        mMessage.setCardReaderStatus(cardReaderStatus);
        mMessage.setStatusChange(statusChange);
        mMessage.setTransportProtocol(transportProtocol);
        mMessage.setApdu(apdu);
        mMessage.setApdu7816(apdu7816);
        mMessage.setApduResp(apduResp);
        mMessage.setAtr(atr);

        ByteArrayOutputStream os = new ByteArrayOutputStream();
        mMessage.write(os);

        // Now, reconstruct the message from the written data.
        byte[] data = os.toByteArray();
        ByteArrayInputStream is = new ByteArrayInputStream(data);
        int msgTypeReadFromStream = is.read();
        SapMessage msgFromInputStream = SapMessage.readMessage(msgTypeReadFromStream, is);

        assertThat(msgFromInputStream.getMsgType()).isEqualTo(msgType);
        assertThat(msgFromInputStream.getMaxMsgSize()).isEqualTo(maxMsgSize);
        assertThat(msgFromInputStream.getConnectionStatus()).isEqualTo(connectionStatus);
        assertThat(msgFromInputStream.getResultCode()).isEqualTo(resultCode);
        assertThat(msgFromInputStream.getDisconnectionType()).isEqualTo(disconnectionType);
        assertThat(msgFromInputStream.getCardReaderStatus()).isEqualTo(cardReaderStatus);
        assertThat(msgFromInputStream.getStatusChange()).isEqualTo(statusChange);
        assertThat(msgFromInputStream.getTransportProtocol()).isEqualTo(transportProtocol);
        assertThat(msgFromInputStream.getApdu()).isEqualTo(apdu);
        assertThat(msgFromInputStream.getApdu7816()).isEqualTo(apdu7816);
        assertThat(msgFromInputStream.getApduResp()).isEqualTo(apduResp);
        assertThat(msgFromInputStream.getAtr()).isEqualTo(atr);
    }

    // TODO: Add test for newInstance()
    // Note: MsgHeader throws a NoSuchMethodError when MsgHeader.getType() is called,
    //       which prevents writing tests for newInstance. Possibly a bug with protobuf?

    @Test
    public void send() throws Exception {
        int maxMsgSize = 512;
        byte[] apdu = new byte[]{0x01, 0x02};
        byte[] apdu7816 = new byte[]{0x03, 0x04};

        ISapRilReceiver sapProxy = mock(ISapRilReceiver.class);
        mMessage.setClearRilQueue(true);

        mMessage.setMsgType(ID_CONNECT_REQ);
        mMessage.setMaxMsgSize(maxMsgSize);
        mMessage.send(sapProxy);
        verify(sapProxy).connectReq(anyInt(), eq(maxMsgSize));
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_DISCONNECT_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).disconnectReq(anyInt());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_TRANSFER_APDU_REQ);
        mMessage.setApdu(apdu);
        mMessage.send(sapProxy);
        verify(sapProxy).apduReq(anyInt(), anyInt(), any());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_TRANSFER_APDU_REQ);
        mMessage.setApdu(null);
        mMessage.setApdu7816(apdu7816);
        mMessage.send(sapProxy);
        verify(sapProxy).apduReq(anyInt(), anyInt(), any());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_TRANSFER_APDU_REQ);
        mMessage.setApdu(null);
        mMessage.setApdu7816(null);
        assertThrows(IllegalArgumentException.class, () -> mMessage.send(sapProxy));

        mMessage.setMsgType(ID_SET_TRANSPORT_PROTOCOL_REQ);
        mMessage.setTransportProtocol(TRANS_PROTO_T0);
        mMessage.send(sapProxy);
        verify(sapProxy).setTransferProtocolReq(anyInt(), eq(SapTransferProtocol.T0));
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_SET_TRANSPORT_PROTOCOL_REQ);
        mMessage.setTransportProtocol(TRANS_PROTO_T1);
        mMessage.send(sapProxy);
        verify(sapProxy).setTransferProtocolReq(anyInt(), eq(SapTransferProtocol.T1));
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_TRANSFER_ATR_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).transferAtrReq(anyInt());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_POWER_SIM_OFF_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).powerReq(anyInt(), eq(false));
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_POWER_SIM_ON_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).powerReq(anyInt(), eq(true));
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_RESET_SIM_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).resetSimReq(anyInt());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(ID_TRANSFER_CARD_READER_STATUS_REQ);
        mMessage.send(sapProxy);
        verify(sapProxy).transferCardReaderStatusReq(anyInt());
        Mockito.clearInvocations(sapProxy);

        mMessage.setMsgType(-1000);
        assertThrows(IllegalArgumentException.class, () -> mMessage.send(sapProxy));
    }
}
