/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.media.MediaMetadata;
import android.media.browse.MediaBrowser.MediaItem;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Bundle;
import android.service.media.MediaBrowserService;
import android.util.Log;

import com.android.bluetooth.R;

import java.util.ArrayList;
import java.util.List;

/**
 * Implements the MediaBrowserService interface to AVRCP and A2DP
 *
 * This service provides a means for external applications to access A2DP and AVRCP.
 * The applications are expected to use MediaBrowser (see API) and all the music
 * browsing/playback/metadata can be controlled via MediaBrowser and MediaController.
 *
 * The current behavior of MediaSession exposed by this service is as follows:
 * 1. MediaSession is active (i.e. SystemUI and other overview UIs can see updates) when device is
 * connected and first starts playing. Before it starts playing we do not active the session.
 * 1.1 The session is active throughout the duration of connection.
 * 2. The session is de-activated when the device disconnects. It will be connected again when (1)
 * happens.
 */
public class BluetoothMediaBrowserService extends MediaBrowserService {
    private static final String TAG = "BluetoothMediaBrowserService";
    private static final boolean DBG = true;

    private static BluetoothMediaBrowserService sBluetoothMediaBrowserService;

    private MediaSession mSession;

    // Browsing related structures.
    private List<MediaSession.QueueItem> mMediaQueue = new ArrayList<>();


    /**
     * Initialize this BluetoothMediaBrowserService, creating our MediaSession, MediaPlayer and
     * MediaMetaData, and setting up mechanisms to talk with the AvrcpControllerService.
     */
    @Override
    public void onCreate() {
        if (DBG) Log.d(TAG, "onCreate");
        super.onCreate();

        // Create and configure the MediaSession
        mSession = new MediaSession(this, TAG);
        setSessionToken(mSession.getSessionToken());
        mSession.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS);
        mSession.setQueueTitle(getString(R.string.bluetooth_a2dp_sink_queue_name));
        mSession.setQueue(mMediaQueue);
        PlaybackState.Builder playbackStateBuilder = new PlaybackState.Builder();
        playbackStateBuilder.setState(PlaybackState.STATE_ERROR,
                PlaybackState.PLAYBACK_POSITION_UNKNOWN, 1.0f).setActions(0);
        playbackStateBuilder.setErrorMessage(getString(R.string.bluetooth_disconnected));
        mSession.setPlaybackState(playbackStateBuilder.build());
        sBluetoothMediaBrowserService = this;
    }

    List<MediaItem> getContents(final String parentMediaId) {
        AvrcpControllerService avrcpControllerService =
                AvrcpControllerService.getAvrcpControllerService();
        if (avrcpControllerService == null) {
            return new ArrayList(0);
        } else {
            return avrcpControllerService.getContents(parentMediaId);
        }
    }

    @Override
    public synchronized void onLoadChildren(final String parentMediaId,
            final Result<List<MediaItem>> result) {
        if (DBG) Log.d(TAG, "onLoadChildren parentMediaId=" + parentMediaId);
        List<MediaItem> contents = getContents(parentMediaId);
        if (contents == null) {
            result.detach();
        } else {
            result.sendResult(contents);
        }
    }

    @Override
    public BrowserRoot onGetRoot(String clientPackageName, int clientUid, Bundle rootHints) {
        if (DBG) Log.d(TAG, "onGetRoot");
        return new BrowserRoot(BrowseTree.ROOT, null);
    }

    private void updateNowPlayingQueue(BrowseTree.BrowseNode node) {
        List<MediaItem> songList = node.getContents();
        mMediaQueue.clear();
        if (songList != null) {
            for (MediaItem song : songList) {
                mMediaQueue.add(new MediaSession.QueueItem(song.getDescription(),
                        mMediaQueue.size()));
            }
        }
        mSession.setQueue(mMediaQueue);
    }

    static synchronized void notifyChanged(BrowseTree.BrowseNode node) {
        if (sBluetoothMediaBrowserService != null) {
            if (node.getScope() == AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING) {
                sBluetoothMediaBrowserService.updateNowPlayingQueue(node);
            } else {
                sBluetoothMediaBrowserService.notifyChildrenChanged(node.getID());
            }
        }
    }

    static synchronized void addressedPlayerChanged(MediaSession.Callback callback) {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setCallback(callback);
        } else {
            Log.w(TAG, "addressedPlayerChanged Unavailable");
        }
    }

    static synchronized void trackChanged(MediaMetadata mediaMetadata) {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setMetadata(mediaMetadata);
        } else {
            Log.w(TAG, "trackChanged Unavailable");
        }
    }

    static synchronized void notifyChanged(PlaybackState playbackState) {
        Log.d(TAG, "notifyChanged PlaybackState" + playbackState);
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setPlaybackState(playbackState);
        } else {
            Log.w(TAG, "notifyChanged Unavailable");
        }
    }

    /**
     * Send AVRCP Play command
     */
    public static synchronized void play() {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.getController().getTransportControls().play();
        } else {
            Log.w(TAG, "play Unavailable");
        }
    }

    /**
     * Send AVRCP Pause command
     */
    public static synchronized void pause() {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.getController().getTransportControls().pause();
        } else {
            Log.w(TAG, "pause Unavailable");
        }
    }

    /**
     * Set Media session active whenever we have Focus of any kind
     */
    public static synchronized void setActive(boolean active) {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setActive(active);
        } else {
            Log.w(TAG, "setActive Unavailable");
        }
    }
}
