/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
import android.util.Log;
import android.util.StatsLog;
import java.util.List;
import java.util.ArrayList;
import java.lang.Boolean;
import java.lang.Class;
import java.lang.Integer;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import android.content.Context;

public class ApmConstIntf {
    public static final String TAG = "APM: ApmConstIntf";

    static Class ApmConst = null;
    static Class AudioFeaturesClass = null;
    static Class AudioProfilesClass = null;
    public static Class StreamAudioService = null;
    public static Class CoordinatedAudioService = null;
    public static Class MusicPlayerControlService = null;
    public static Class MediaPlayerControlService = null;
    public static int LE_AUDIO_UNICAST;
    public static int COORDINATED_AUDIO_UNICAST;
    public static int MUSIC_PLAYER_CONTROL;

    public static String groupAddress;
    public static String CoordinatedAudioServiceName;
    public static String MusicPlayerControlServiceName;

    static {
        try {
            StreamAudioService = Class.forName("com.android.bluetooth.apm.StreamAudioService");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, ex);
        }

        try {
            if(StreamAudioService != null) {
              LE_AUDIO_UNICAST = (Integer)StreamAudioService.getDeclaredField("LE_AUDIO_UNICAST").get(null);
              COORDINATED_AUDIO_UNICAST = (Integer)StreamAudioService.getDeclaredField("COORDINATED_AUDIO_UNICAST").get(null);
              CoordinatedAudioServiceName = (String)StreamAudioService.getDeclaredField("CoordinatedAudioServiceName").get(null);
            }
        } catch (NoSuchFieldException ex) {
            Log.w(TAG, ex);
        } catch (IllegalAccessException ex) {
            Log.w(TAG, ex);
        }

        try {
            if (CoordinatedAudioServiceName != null) {
              CoordinatedAudioService = Class.forName(CoordinatedAudioServiceName);
            }
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, ex);
        }

        try {
            MediaPlayerControlService = Class.forName("com.android.bluetooth.apm.MediaControlManager");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, ex);
        }
        try {
            if (MediaPlayerControlService != null) {
              MusicPlayerControlServiceName = (String)MediaPlayerControlService.getDeclaredField("MusicPlayerControlServiceName").get(null);
              MUSIC_PLAYER_CONTROL = (Integer)MediaPlayerControlService.getDeclaredField("MUSIC_PLAYER_CONTROL").get(null);
            }
        } catch(IllegalAccessException ex) {
            Log.w(TAG, ex);
        } catch (NoSuchFieldException ex) {
            Log.w(TAG, ex);
        }

        try {
            if (MusicPlayerControlServiceName != null) {
              MusicPlayerControlService = Class.forName(MusicPlayerControlServiceName);
            }
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, ex);
        }
   }

    public static class AudioFeatures {
        public static int CALL_AUDIO;
        public static int MEDIA_AUDIO;
        public static int CALL_CONTROL;
        public static int MEDIA_CONTROL;
        public static int MEDIA_VOLUME_CONTROL;
        public static int CALL_VOLUME_CONTROL;;
        public static int BROADCAST_AUDIO;
        public static int HEARING_AID;
        public static int MAX_AUDIO_FEATURES;
    }

    public static class AudioProfiles {
        public static int NONE = 0x0000;
        public static int A2DP;
        public static int HFP;
        public static int AVRCP;

        public static int HAP_BREDR;

        public static int BROADCAST_BREDR;
        public static int BROADCAST_LE;
    }

    protected static void init () {
        try {
            ApmConst = Class.forName("com.android.bluetooth.apm.ApmConst");
        } catch(ClassNotFoundException ex) {
            Log.w(TAG, "Class ApmConst not present. " + ex);
            ApmConst = null;
            return;
        }

        Class[] subClass = ApmConst.getDeclaredClasses();
        for(int i=0; i < subClass.length; i++) {
            if(subClass[i].getName().contains("AudioFeatures")) {
                AudioFeaturesClass = subClass[i];
            } else if(subClass[i].getName().contains("AudioProfiles")) {
                AudioProfilesClass = subClass[i];
            }
        }

        try {
            AudioFeatures.CALL_AUDIO = (Integer)AudioFeaturesClass.getDeclaredField("CALL_AUDIO").get(null);
            AudioFeatures.MEDIA_AUDIO = (Integer)AudioFeaturesClass.getDeclaredField("MEDIA_AUDIO").get(null);
            AudioFeatures.CALL_CONTROL = (Integer)AudioFeaturesClass.getDeclaredField("CALL_CONTROL").get(null);
            AudioFeatures.MEDIA_CONTROL = (Integer)AudioFeaturesClass.getDeclaredField("MEDIA_CONTROL").get(null);
            AudioFeatures.MEDIA_VOLUME_CONTROL = (Integer)AudioFeaturesClass.getDeclaredField("MEDIA_VOLUME_CONTROL").get(null);
            AudioFeatures.CALL_VOLUME_CONTROL = (Integer)AudioFeaturesClass.getDeclaredField("CALL_VOLUME_CONTROL").get(null);
            AudioFeatures.BROADCAST_AUDIO = (Integer)AudioFeaturesClass.getDeclaredField("BROADCAST_AUDIO").get(null);
            AudioFeatures.HEARING_AID = (Integer)AudioFeaturesClass.getDeclaredField("HEARING_AID").get(null);
            AudioFeatures.MAX_AUDIO_FEATURES = (Integer)AudioFeaturesClass.getDeclaredField("MAX_AUDIO_FEATURES").get(null);

            AudioProfiles.A2DP = (Integer)AudioProfilesClass.getDeclaredField("A2DP").get(null);
            AudioProfiles.HFP = (Integer)AudioProfilesClass.getDeclaredField("HFP").get(null);
            AudioProfiles.AVRCP = (Integer)AudioProfilesClass.getDeclaredField("AVRCP").get(null);
            AudioProfiles.HAP_BREDR = (Integer)AudioProfilesClass.getDeclaredField("HAP_BREDR").get(null);
            AudioProfiles.BROADCAST_BREDR = (Integer)AudioProfilesClass.getDeclaredField("BROADCAST_BREDR").get(null);
            AudioProfiles.BROADCAST_LE = (Integer)AudioProfilesClass.getDeclaredField("BROADCAST_LE").get(null);

            groupAddress = (String)ApmConst.getDeclaredField("groupAddress").get(null);
        } catch (NoSuchFieldException ex) {
            Log.w(TAG, ex);
        } catch (IllegalAccessException ex) {
            Log.w(TAG, ex);
        }
    }

    public static boolean getAospLeaEnabled() {
        if(ApmConst == null)
            return false;

        try {
            Method getAospLeaEnabled = ApmConst.getDeclaredMethod("getAospLeaEnabled");
            Boolean ret = (Boolean)getAospLeaEnabled.invoke(null);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    }

    public static boolean getQtiLeAudioEnabled() {
        if(ApmConst == null)
            return false;

        try {
            Method getQtiLeAudioEnabled = ApmConst.getDeclaredMethod("getQtiLeAudioEnabled");
            Boolean ret = (Boolean)getQtiLeAudioEnabled.invoke(null);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return false;
    }

    public static boolean getActiveLeMedia() {
        if(ApmConst == null)
            return false;

        try {
            Method getActiveLeMedia = ApmConst.getDeclaredMethod("getActiveLeMedia");
            Boolean ret = (Boolean)getActiveLeMedia.invoke(null);
            return ret;
        } catch(IllegalAccessException e) {
            Log.i(TAG, "Exception" + e);
        } catch(NoSuchMethodException e) {
            Log.i(TAG, "Exception" + e);
        } catch(InvocationTargetException e) {
            Log.i(TAG, "Exception" + e);
        }

        return true;
    }
}
