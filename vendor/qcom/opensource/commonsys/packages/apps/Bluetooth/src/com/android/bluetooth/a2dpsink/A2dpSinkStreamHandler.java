/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.bluetooth.a2dpsink;

import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.android.bluetooth.R;
import com.android.bluetooth.avrcpcontroller.BluetoothMediaBrowserService;
import com.android.bluetooth.hfpclient.HeadsetClientService;

import java.util.List;

/**
 * Bluetooth A2DP SINK Streaming Handler.
 *
 * This handler defines how the stack behaves once the A2DP connection is established and both
 * devices are ready for streaming. For simplification we assume that the connection can either
 * stream music immediately (i.e. data packets coming in or have potential to come in) or it cannot
 * stream (i.e. Idle and Open states are treated alike). See Fig 4-1 of GAVDP Spec 1.0.
 *
 * Note: There are several different audio tracks that a connected phone may like to transmit over
 * the A2DP stream including Music, Navigation, Assistant, and Notifications.  Music is the only
 * track that is almost always accompanied with an AVRCP play/pause command.
 *
 * Streaming is initiated by either an explicit play command from user interaction or audio coming
 * from the phone.  Streaming is terminated when either the user pauses the audio, the audio stream
 * from the phone ends, the phone disconnects, or audio focus is lost.  During playback if there is
 * a change to audio focus playback may be temporarily paused and then resumed when focus is
 * restored.
 */
public class A2dpSinkStreamHandler extends Handler {
    private static final String TAG = "A2dpSinkStreamHandler";
    private static final boolean DBG = true;

    // Configuration Variables
    private static final int DEFAULT_DUCK_PERCENT = 25;
    private static final int SETTLE_TIMEOUT = 400;

    // Incoming events.
    public static final int SRC_STR_START = 0; // Audio stream from remote device started
    public static final int SRC_STR_STOP = 1; // Audio stream from remote device stopped
    public static final int SNK_PLAY = 2; // Play command was generated from local device
    public static final int SNK_PAUSE = 3; // Pause command was generated from local device
    public static final int SRC_PLAY = 4; // Play command was generated from remote device
    public static final int SRC_PAUSE = 5; // Pause command was generated from remote device
    public static final int DISCONNECT = 6; // Remote device was disconnected
    public static final int AUDIO_FOCUS_CHANGE = 7; // Audio focus callback with associated change
    public static final int REQUEST_FOCUS = 8; // Request focus when the media service is active
    public static final int DELAYED_PAUSE = 9; // If a call just started allow stack time to settle
    public static final int RELEASE_FOCUS = 10;

    // Used to indicate focus lost
    private static final int STATE_FOCUS_LOST = 0;
    // Used to inform bluedroid that focus is granted
    private static final int STATE_FOCUS_GRANTED = 1;

    // Private variables.
    private A2dpSinkService mA2dpSinkService;
    private Context mContext;
    private AudioManager mAudioManager;
    // Keep track if the remote device is providing audio
    private boolean mStreamAvailable = false;
    private boolean mSentPause = false;
    // Keep track of the relevant audio focus (None, Transient, Gain)
    private int mAudioFocus = AudioManager.AUDIOFOCUS_NONE;

    // In order for Bluetooth to be considered as an audio source capable of receiving media key
    // events (In the eyes of MediaSessionService), we need an active MediaPlayer in addition to a
    // MediaSession. Because of this, the media player below plays an incredibly short, silent audio
    // sample so that MediaSessionService and AudioPlaybackStateMonitor will believe that we're the
    // current active player and send the Bluetooth process media events. This allows AVRCP
    // controller to create a MediaSession and handle the events if it would like. The player and
    // session requirement is a restriction currently imposed by the media framework code and could
    // be reconsidered in the future.
    private MediaPlayer mMediaPlayer = null;

    // Focus changes when we are currently holding focus.
    private OnAudioFocusChangeListener mAudioFocusListener = new OnAudioFocusChangeListener() {
        @Override
        public void onAudioFocusChange(int focusChange) {
            if (DBG) {
                Log.d(TAG, "onAudioFocusChangeListener focuschange " + focusChange);
            }
            A2dpSinkStreamHandler.this.obtainMessage(AUDIO_FOCUS_CHANGE, focusChange)
                    .sendToTarget();
        }
    };

    public A2dpSinkStreamHandler(A2dpSinkService a2dpSinkService, Context context) {
        mA2dpSinkService = a2dpSinkService;
        mContext = context;
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    }

    void requestAudioFocus(boolean request) {
        obtainMessage(REQUEST_FOCUS, request).sendToTarget();
    }

    int getFocusState() {
        return mAudioFocus;
    }

    boolean isPlaying() {
        return (mStreamAvailable
                && (mAudioFocus == AudioManager.AUDIOFOCUS_GAIN
                || mAudioFocus == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK));
    }

    @Override
    public void handleMessage(Message message) {
        if (DBG) {
            Log.d(TAG, " process message: " + message.what);
            Log.d(TAG, " audioFocus =  " + mAudioFocus);
        }
        switch (message.what) {
            case SRC_STR_START:
                mStreamAvailable = true;
                Log.d(TAG, " isTvDevice =  " + isTvDevice() +
                          "shouldRequestFocus = " + shouldRequestFocus());
                if (isTvDevice() || shouldRequestFocus()) {
                    requestAudioFocusIfNone();
                }

                // Audio stream has started, stop it if we don't have focus.
                if (mAudioFocus == AudioManager.AUDIOFOCUS_NONE) {
                    requestAudioFocus();
                }
                break;

            case SRC_STR_STOP:
                // Audio stream has stopped, maintain focus but stop avrcp updates.
                break;

            case SNK_PLAY:
                // Local play command, gain focus and start avrcp updates.
                requestAudioFocusIfNone();
                break;

            case SNK_PAUSE:
                mStreamAvailable = false;
                // Local pause command, maintain focus but stop avrcp updates.
                break;

            case SRC_PLAY:
                mStreamAvailable = true;
                // Remote play command.
                if (isIotDevice() || isTvDevice() || shouldRequestFocus()) {
                    requestAudioFocusIfNone();
                    break;
                }

                // Audio stream has started, stop it if we don't have focus.
                if (mAudioFocus == AudioManager.AUDIOFOCUS_NONE) {
                    requestAudioFocus();
                }
                break;

            case SRC_PAUSE:
                mStreamAvailable = false;
                // Remote pause command, stop avrcp updates.
                break;

            case REQUEST_FOCUS:
                requestAudioFocusIfNone();
                break;

            case RELEASE_FOCUS:
                abandonAudioFocus();
                break;

            case DISCONNECT:
                // Remote device has disconnected, restore everything to default state.
                mSentPause = false;
                break;

            case AUDIO_FOCUS_CHANGE:
                // message.obj is the newly granted audio focus.
                switch ((int) message.obj) {
                    case AudioManager.AUDIOFOCUS_GAIN:
                        removeMessages(DELAYED_PAUSE);
                        // Begin playing audio, if we paused the remote, send a play now.
                        startFluorideStreaming();
                        if (mSentPause) {
                            sendAvrcpPlay();
                            mSentPause = false;
                        }
                        break;

                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                        // Make the volume duck.
                        int duckPercent = mContext.getResources()
                                .getInteger(R.integer.a2dp_sink_duck_percent);
                        if (duckPercent < 0 || duckPercent > 100) {
                            Log.e(TAG, "Invalid duck percent using default.");
                            duckPercent = DEFAULT_DUCK_PERCENT;
                        }
                        float duckRatio = (duckPercent / 100.0f);
                        if (DBG) {
                            Log.d(TAG, "Setting reduce gain on transient loss gain=" + duckRatio);
                        }
                        setFluorideAudioTrackGain(duckRatio);
                        break;

                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                        // Temporary loss of focus, if we are actively streaming pause the remote
                        // and make sure we resume playback when we regain focus.
                        sendMessageDelayed(obtainMessage(DELAYED_PAUSE), SETTLE_TIMEOUT);
                        setFluorideAudioTrackGain(0);
                        break;

                    case AudioManager.AUDIOFOCUS_LOSS:
                        // Permanent loss of focus probably due to another audio app, abandon focus
                        // and stop playback.
                        abandonAudioFocus();
                        sendAvrcpPause();
                        break;
                }
                break;

            case DELAYED_PAUSE:
                if (mStreamAvailable && !inCallFromStreamingDevice()) {
                    sendAvrcpPause();
                    mSentPause = true;
                    mStreamAvailable = false;
                }
                break;

            default:
                Log.w(TAG, "Received unexpected event: " + message.what);
        }
    }

    /**
     * Utility functions.
     */
    private void requestAudioFocusIfNone() {
        if (DBG) Log.d(TAG, "requestAudioFocusIfNone()");
        if (mAudioFocus == AudioManager.AUDIOFOCUS_NONE) {
            Log.d(TAG, " mAudioFocus =  " + mAudioFocus);
            requestAudioFocus();
        }
        // On the off change mMediaPlayer errors out and dies, we want to make sure we retry this.
        // This function immediately exits if we have a MediaPlayer object.
        requestMediaKeyFocus();
    }

    private synchronized int requestAudioFocus() {
        // Bluetooth A2DP may carry Music, Audio Books, Navigation, or other sounds so mark content
        // type unknown.
        Log.d(TAG, " requestAudioFocus() ");
        AudioAttributes streamAttributes =
                new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
                        .build();
        // Bluetooth ducking is handled at the native layer so tell the Audio Manger to notify the
        // focus change listener via .setWillPauseWhenDucked().
        AudioFocusRequest focusRequest =
                new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(
                        streamAttributes)
                        .setWillPauseWhenDucked(true)
                        .setOnAudioFocusChangeListener(mAudioFocusListener, this)
                        .build();
        int focusRequestStatus = mAudioManager.requestAudioFocus(focusRequest);
        Log.d(TAG, " focusRequestStatus =  " + focusRequestStatus +
                                 " focusRequest: " + focusRequest);
        // If the request is granted begin streaming immediately and schedule an upgrade.
        if (focusRequestStatus == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            startFluorideStreaming();
            mAudioFocus = AudioManager.AUDIOFOCUS_GAIN;
        }
        return focusRequestStatus;
    }

    /**
     * Creates a MediaPlayer that plays a silent audio sample so that MediaSessionService will be
     * aware of the fact that Bluetooth is playing audio.
     *
     * This allows the MediaSession in AVRCP Controller to be routed media key events, if we've
     * chosen to use it.
     */
    private synchronized void requestMediaKeyFocus() {
        if (DBG) Log.d(TAG, "requestMediaKeyFocus()");

        if (mMediaPlayer != null) return;

        AudioAttributes attrs = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build();

        mMediaPlayer = MediaPlayer.create(mContext, R.raw.silent, attrs,
                mAudioManager.generateAudioSessionId());
        if (mMediaPlayer == null) {
            Log.e(TAG, "Failed to initialize media player. You may not get media key events");
            return;
        }

        mMediaPlayer.setLooping(false);
        mMediaPlayer.setOnErrorListener((mp, what, extra) -> {
            Log.e(TAG, "Silent media player error: " + what + ", " + extra);
            releaseMediaKeyFocus();
            return false;
        });

        mMediaPlayer.start();
        BluetoothMediaBrowserService.setActive(true);
    }

    private synchronized void abandonAudioFocus() {
        stopFluorideStreaming();
        if (mAudioFocus != AudioManager.AUDIOFOCUS_NONE) {
            Log.d(TAG, "abandoning audio focus");
            AudioAttributes streamAttributes =
                  new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                          .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
                          .build();
            AudioFocusRequest mfocusRequest =
                  new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(
                          streamAttributes)
                          .setWillPauseWhenDucked(true)
                          .setOnAudioFocusChangeListener(mAudioFocusListener, this)
                          .build();
            mAudioManager.abandonAudioFocusRequest(mfocusRequest);
            mAudioFocus = AudioManager.AUDIOFOCUS_NONE;
        }
    }

    /**
     * Destroys the silent audio sample MediaPlayer, notifying MediaSessionService of the fact
     * we're no longer playing audio.
     */
    private synchronized void releaseMediaKeyFocus() {
        if (DBG) Log.d(TAG, "releaseMediaKeyFocus()");
        if (mMediaPlayer == null) {
            return;
        }
        BluetoothMediaBrowserService.setActive(false);
        mMediaPlayer.stop();
        mMediaPlayer.release();
        mMediaPlayer = null;
    }

    private void startFluorideStreaming() {
        mA2dpSinkService.informAudioFocusStateNative(STATE_FOCUS_GRANTED);
        mA2dpSinkService.informAudioTrackGainNative(1.0f);
    }

    private void stopFluorideStreaming() {
        mA2dpSinkService.informAudioFocusStateNative(STATE_FOCUS_LOST);
    }

    private void setFluorideAudioTrackGain(float gain) {
        mA2dpSinkService.informAudioTrackGainNative(gain);
    }

    private void sendAvrcpPause() {
        BluetoothMediaBrowserService.pause();
    }

    private void sendAvrcpPlay() {
        BluetoothMediaBrowserService.play();
    }

    private boolean inCallFromStreamingDevice() {
        BluetoothDevice targetDevice = null;
        List<BluetoothDevice> connectedDevices = mA2dpSinkService.getConnectedDevices();
        if (!connectedDevices.isEmpty()) {
            targetDevice = connectedDevices.get(0);
        }
        HeadsetClientService headsetClientService = HeadsetClientService.getHeadsetClientService();
        if (targetDevice != null && headsetClientService != null) {
            return headsetClientService.getCurrentCalls(targetDevice).size() > 0;
        }
        return false;
    }

    synchronized int getAudioFocus() {
        return mAudioFocus;
    }

    private boolean isIotDevice() {
        return mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_EMBEDDED);
    }

    private boolean isTvDevice() {
        return mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK);
    }

    private boolean shouldRequestFocus() {
        return mContext.getResources()
                .getBoolean(R.bool.a2dp_sink_automatically_request_audio_focus);
    }

}
