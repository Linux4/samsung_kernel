/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth.lebroadcast;

import android.bluetooth.BluetoothLeAudioContentMetadata;
import android.bluetooth.BluetoothLeBroadcastMetadata;
import android.bluetooth.IBluetoothLeBroadcastCallback;
import android.content.AttributionSource;
import android.util.Log;

import com.android.modules.utils.SynchronousResultReceiver;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/* LE Broadcast Service Reflect Interface */
public class LeBroadcastServIntf {
    public static final String TAG = "BroadcastService: LeBroadcastServIntf";

    private static LeBroadcastServIntf mInterface = null;
    static Class LeBroadcastService = null;
    static Object mLeBroadcastService = null;

    private LeBroadcastServIntf() {
    }

    public static LeBroadcastServIntf get() {
        if(mInterface == null) {
            mInterface = new LeBroadcastServIntf();
        }
        return mInterface;
    }

    public static void init (Object obj) {
        Log.i(TAG, "init");
        mLeBroadcastService = obj;

        try {
            LeBroadcastService = Class.forName(
                    "com.android.bluetooth.broadcast.BroadcastService");
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Class BroadcastService not present. " + e);
            LeBroadcastService = null;
            mLeBroadcastService = null;
        }
    }

    public void registerLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
            AttributionSource source, SynchronousResultReceiver receiver) {
        Log.i(TAG, "registerLeBroadcastCallback");
        if (LeBroadcastService == null)
            return;
        Class[] args = new Class[3];
        args[0] = IBluetoothLeBroadcastCallback.class;
        args[1] = AttributionSource.class;
        args[2] = SynchronousResultReceiver.class;
        try {
            Method registerLeBroadcastCallback =
                    LeBroadcastService.getDeclaredMethod("registerLeBroadcastCallback", args);
            registerLeBroadcastCallback.invoke(mLeBroadcastService, callback, source, receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void unregisterLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
            AttributionSource source, SynchronousResultReceiver receiver) {
        Log.i(TAG, "unregisterLeBroadcastCallback");
        if (LeBroadcastService == null)
            return;
        Class[] args = new Class[3];
        args[0] = IBluetoothLeBroadcastCallback.class;
        args[1] = AttributionSource.class;
        args[2] = SynchronousResultReceiver.class;
        try {
            Method unregisterLeBroadcastCallback =
                    LeBroadcastService.getDeclaredMethod("unregisterLeBroadcastCallback", args);
            unregisterLeBroadcastCallback.invoke(mLeBroadcastService, callback, source, receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void startBroadcast(BluetoothLeAudioContentMetadata contentMetadata,
            byte[] broadcastCode, AttributionSource source) {
        Log.i(TAG, "startBroadcast");
        if (LeBroadcastService == null)
            return;
        Class[] args = new Class[3];
        args[0] = BluetoothLeAudioContentMetadata.class;
        args[1] = byte[].class;
        args[2] = AttributionSource.class;
        try {
            Method startBroadcast =
                    LeBroadcastService.getDeclaredMethod("startBroadcast", args);
            startBroadcast.invoke(mLeBroadcastService, contentMetadata, broadcastCode, source);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void stopBroadcast(int broadcastId, AttributionSource source) {
        Log.i(TAG, "stopBroadcast");
        if (LeBroadcastService == null)
            return;
        Class[] args = new Class[2];
        args[0] = int.class;
        args[1] = AttributionSource.class;
        try {
            Method stopBroadcast =
                    LeBroadcastService.getDeclaredMethod("stopBroadcast", args);
            stopBroadcast.invoke(mLeBroadcastService, broadcastId, source);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public void updateBroadcast(int broadcastId,
            BluetoothLeAudioContentMetadata contentMetadata, AttributionSource source) {
        Log.i(TAG, "updateBroadcast");
        if (LeBroadcastService == null)
            return;
        Class[] args = new Class[3];
        args[0] = int.class;
        args[1] = BluetoothLeAudioContentMetadata.class;
        args[2] = AttributionSource.class;
        try {
            Method updateBroadcast =
                    LeBroadcastService.getDeclaredMethod("updateBroadcast", args);
            updateBroadcast.invoke(mLeBroadcastService, broadcastId, contentMetadata, source);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
    }

    public boolean isPlaying(int broadcastId, AttributionSource source,
            SynchronousResultReceiver receiver) {
        Log.i(TAG, "isPlaying");
        if (LeBroadcastService == null)
            return false;
        Class[] args = new Class[3];
        args[0] = int.class;
        args[1] = AttributionSource.class;
        args[2] = SynchronousResultReceiver.class;
        try {
            Method isPlaying =
                    LeBroadcastService.getDeclaredMethod("isPlaying", args);
            return (boolean) isPlaying.invoke(mLeBroadcastService, broadcastId, source, receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return false;
    }

    public List<BluetoothLeBroadcastMetadata> getAllBroadcastMetadata(AttributionSource source,
            SynchronousResultReceiver receiver) {
        Log.i(TAG, "getAllBroadcastMetadata");
        ArrayList<BluetoothLeBroadcastMetadata> metaData = new ArrayList<>();
        if (LeBroadcastService == null)
            return metaData;
        Class[] args = new Class[2];
        args[0] = AttributionSource.class;
        args[1] = SynchronousResultReceiver.class;
        try {
            Method getAllBroadcastMetadata =
                    LeBroadcastService.getDeclaredMethod("getAllBroadcastMetadata", args);
            return (List<BluetoothLeBroadcastMetadata>)
                    getAllBroadcastMetadata.invoke(mLeBroadcastService, source, receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return metaData;
    }

    public int getMaximumNumberOfBroadcasts(AttributionSource source,
            SynchronousResultReceiver receiver) {
        Log.i(TAG, "getMaximumNumberOfBroadcasts");
        if (LeBroadcastService == null)
            return 0;
        Class[] args = new Class[2];
        args[0] = AttributionSource.class;
        args[1] = SynchronousResultReceiver.class;
        try {
            Method getMaximumNumberOfBroadcasts =
                    LeBroadcastService.getDeclaredMethod("getMaximumNumberOfBroadcasts", args);
            return (int) getMaximumNumberOfBroadcasts.invoke(mLeBroadcastService, source, receiver);
        } catch (ReflectiveOperationException e) {
            Log.e(TAG, "Exception:" + Log.getStackTraceString(e));
        }
        return 0;
    }
}
