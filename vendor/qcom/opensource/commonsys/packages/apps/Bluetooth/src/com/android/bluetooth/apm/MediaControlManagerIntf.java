/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
         copyright notice, this list of conditions and the following
         disclaimer in the documentation and/or other materials provided
         with the distribution.
       * Neither the name of The Linux Foundation nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

package com.android.bluetooth.apm;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.MediaSession.QueueItem;
import android.media.session.PlaybackState;
import android.util.Log;
import android.util.StatsLog;
import java.util.List;
import java.util.ArrayList;
import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class MediaControlManagerIntf {
    public static final String TAG = "APM: MediaControlIntf";
    private static MediaControlManagerIntf mInterface = null;

    static Class MediaControlManager = null;
    static Object mMediaControl = null;

    Method onMetadataChanged;
    Method onPlaybackStateChanged;
    Method onQueueChanged;
    Method onPackageChanged;
    Method onSessionDestroyed;

    private MediaControlManagerIntf() {

    }

    public static MediaControlManagerIntf get() {
        if(mInterface == null) {
            mInterface = new MediaControlManagerIntf();
        }
        return mInterface;
    }

    protected static void init (Object obj) {
        Log.i(TAG, "init");

        if(mInterface == null) {
            mInterface = new MediaControlManagerIntf();
        }

        mMediaControl = obj;

        try {
            MediaControlManager = Class.forName("com.android.bluetooth.apm.MediaControlManager");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class MediaAudio not present. " + ex);
            MediaControlManager = null;
            mMediaControl = null;
        }

        Class[] onMetadataChanged_arg = new Class[1];
        onMetadataChanged_arg[0] = MediaMetadata.class;

        Class[] onPlaybackStateChanged_arg = new Class[1];
        onPlaybackStateChanged_arg[0] = PlaybackState.class;

        Class[] onQueueChanged_arg = new Class[1];
        onQueueChanged_arg[0] = List.class;

        Class[] onPackageChanged_arg = new Class[1];
        onPackageChanged_arg[0] = String.class;

        Class[] onSessionDestroyed_arg = new Class[1];
        onSessionDestroyed_arg[0] = String.class;

        try {
            mInterface.onMetadataChanged = MediaControlManager.getDeclaredMethod("onMetadataChanged", onMetadataChanged_arg);
            mInterface.onPlaybackStateChanged = MediaControlManager.getDeclaredMethod("onPlaybackStateChanged", onPlaybackStateChanged_arg);
            mInterface.onQueueChanged = MediaControlManager.getDeclaredMethod("onQueueChanged", onQueueChanged_arg);
            mInterface.onPackageChanged = MediaControlManager.getDeclaredMethod("onPackageChanged", onPackageChanged_arg);
            mInterface.onSessionDestroyed = MediaControlManager.getDeclaredMethod("onSessionDestroyed", onSessionDestroyed_arg);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onMetadataChanged(MediaMetadata metadata) {
        if(MediaControlManager == null)
            return;

        try {
            onMetadataChanged.invoke(mMediaControl, metadata);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onPlaybackStateChanged(PlaybackState state) {
        if(MediaControlManager == null)
            return;

        try {
            onPlaybackStateChanged.invoke(mMediaControl, state);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onQueueChanged(List<MediaSession.QueueItem> queue) {
        if(MediaControlManager == null)
            return;

        try {
            onQueueChanged.invoke(mMediaControl, queue);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onPackageChanged(String packageName) {
        if(MediaControlManager == null)
            return;

        try {
            onPackageChanged.invoke(mMediaControl, packageName);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }

    public void onSessionDestroyed(String packageName) {
        if(MediaControlManager == null)
            return;

        try {
            onSessionDestroyed.invoke(mMediaControl, packageName);
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }
    }
}

