/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.bluetooth.telephony;

import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.telecom.Call;
import android.telecom.GatewayInfo;
import android.telecom.InCallService;
import android.telecom.PhoneAccountHandle;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;

/**
 * A proxy class of android.telecom.Call that
 * 1) facilitates testing of the BluetoothInCallService class; We can't mock the final class
 * Call directly;
 * 2) Some helper functions, to let Call have same methods as com.android.server.telecom.Call
 *
 * This is necessary due to the "final" attribute of the Call class. In order to
 * test the correct functioning of the BluetoothInCallService class, the final class must be put
 * into a container that can be mocked correctly.
 */
@VisibleForTesting
public class BluetoothCall {

    private Call mCall;

    public Call getCall() {
        return mCall;
    }

    public void setCall(Call call) {
        mCall = call;
    }

    public BluetoothCall(Call call) {
        mCall = call;
    }

    public String getRemainingPostDialSequence() {
        return mCall.getRemainingPostDialSequence();
    }

    public void answer(int videoState) {
        mCall.answer(videoState);
    }

    public void deflect(Uri address) {
        mCall.deflect(address);
    }

    public void reject(boolean rejectWithMessage, String textMessage) {
        mCall.reject(rejectWithMessage, textMessage);
    }

    public void disconnect() {
        mCall.disconnect();
    }

    public void hold() {
        mCall.hold();
    }

    public void unhold() {
        mCall.unhold();
    }

    public void enterBackgroundAudioProcessing() {
        mCall.enterBackgroundAudioProcessing();
    }

    public void exitBackgroundAudioProcessing(boolean shouldRing) {
        mCall.exitBackgroundAudioProcessing(shouldRing);
    }

    public void playDtmfTone(char digit) {
        mCall.playDtmfTone(digit);
    }

    public void stopDtmfTone() {
        mCall.stopDtmfTone();
    }

    public void postDialContinue(boolean proceed) {
        mCall.postDialContinue(proceed);
    }

    public void phoneAccountSelected(PhoneAccountHandle accountHandle, boolean setDefault) {
        mCall.phoneAccountSelected(accountHandle, setDefault);
    }

    public void conference(BluetoothCall callToConferenceWith) {
        if (callToConferenceWith != null) {
            mCall.conference(callToConferenceWith.getCall());
        }
    }

    public void splitFromConference() {
        mCall.splitFromConference();
    }

    public void mergeConference() {
        mCall.mergeConference();
    }

    public void swapConference() {
        mCall.swapConference();
    }

    public void pullExternalCall() {
        mCall.pullExternalCall();
    }

    public void sendCallEvent(String event, Bundle extras) {
        mCall.sendCallEvent(event, extras);
    }

    public void sendRttRequest() {
        mCall.sendRttRequest();
    }

    public void respondToRttRequest(int id, boolean accept) {
        mCall.respondToRttRequest(id, accept);
    }

    public void handoverTo(PhoneAccountHandle toHandle, int videoState, Bundle extras) {
        mCall.handoverTo(toHandle, videoState, extras);
    }

    public void stopRtt() {
        mCall.stopRtt();
    }

    public void putExtras(Bundle extras) {
        mCall.putExtras(extras);
    }

    public void putExtra(String key, boolean value) {
        mCall.putExtra(key, value);
    }

    public void putExtra(String key, int value) {
        mCall.putExtra(key, value);
    }

    public void putExtra(String key, String value) {
        mCall.putExtra(key, value);
    }

    public void removeExtras(List<String> keys) {
        mCall.removeExtras(keys);
    }

    public void removeExtras(String... keys) {
        mCall.removeExtras(keys);
    }

    public String getParentId() {
        Call parent = mCall.getParent();
        if (parent != null) {
            return parent.getDetails().getTelecomCallId();
        }
        return null;
    }

    public List<String> getChildrenIds() {
        return getIds(mCall.getChildren());
    }

    public List<String> getConferenceableCalls() {
        return getIds(mCall.getConferenceableCalls());
    }

    public int getState() {
        return mCall.getState();
    }

    public List<String> getCannedTextResponses() {
        return mCall.getCannedTextResponses();
    }

    public InCallService.VideoCall getVideoCall() {
        return mCall.getVideoCall();
    }

    public Call.Details getDetails() {
        return mCall.getDetails();
    }

    public Call.RttCall getRttCall() {
        return mCall.getRttCall();
    }

    public boolean isRttActive() {
        return mCall.isRttActive();
    }

    public void registerCallback(Call.Callback callback) {
        mCall.registerCallback(callback);
    }

    public void registerCallback(Call.Callback callback, Handler handler) {
        mCall.registerCallback(callback, handler);
    }

    public void unregisterCallback(Call.Callback callback) {
        mCall.unregisterCallback(callback);
    }

    public String toString() {
        String string = mCall.toString();
        return string == null ? "" : string;
    }

    public void addListener(Call.Listener listener) {
        mCall.addListener(listener);
    }

    public void removeListener(Call.Listener listener) {
        mCall.removeListener(listener);
    }

    public String getGenericConferenceActiveChildCallId() {
        if (mCall.getGenericConferenceActiveChildCall() != null) {
           return mCall.getGenericConferenceActiveChildCall().getDetails().getTelecomCallId();
        }
        return null;
    }

    public String getContactDisplayName() {
        return mCall.getDetails().getContactDisplayName();
    }

    public PhoneAccountHandle getAccountHandle() {
        return mCall.getDetails().getAccountHandle();
    }

    public int getVideoState() {
        return mCall.getDetails().getVideoState();
    }

    public String getCallerDisplayName() {
        return mCall.getDetails().getCallerDisplayName();
    }

    @Override
    public boolean equals(Object o) {
        if (o == null) {
            return getCall() == null;
        }
        return o instanceof BluetoothCall && getCall() == ((BluetoothCall) o).getCall();
    }

    // helper functions
    public boolean isSilentRingingRequested() {
        return getDetails().getExtras() != null
                && getDetails().getExtras().getBoolean(Call.EXTRA_SILENT_RINGING_REQUESTED);
    }

    public boolean isConference() {
        return getDetails().hasProperty(Call.Details.PROPERTY_CONFERENCE);
    }

    public boolean can(int capability) {
        return getDetails().can(capability);
    }

    public Uri getHandle() {
        return getDetails().getHandle();
    }

    public GatewayInfo getGatewayInfo() {
        return getDetails().getGatewayInfo();
    }

    public boolean isIncoming() {
        return getDetails().getCallDirection() == Call.Details.DIRECTION_INCOMING;
    }

    public boolean isExternalCall() {
        return getDetails().hasProperty(Call.Details.PROPERTY_IS_EXTERNAL_CALL);
    }

    public boolean isHighDefAudio() {
         return getDetails().hasProperty(Call.Details.PROPERTY_HIGH_DEF_AUDIO);
     }

    public boolean isWifi() {
         return getDetails().hasProperty(Call.Details.PROPERTY_WIFI);
     }

    public String getTelecomCallId() {
        return getDetails().getTelecomCallId();
    }

    public boolean wasConferencePreviouslyMerged() {
        return can(Call.Details.CAPABILITY_SWAP_CONFERENCE) &&
                !can(Call.Details.CAPABILITY_MERGE_CONFERENCE);
    }

    public static List<String> getIds(List<Call> calls) {
        List<String> result = new ArrayList<>();
        for (Call call : calls) {
            if (call != null && call.getDetails() != null) {
                result.add(call.getDetails().getTelecomCallId());
            }
        }
        return result;
    }
}
