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

package com.android.bluetooth.avrcpcontroller;

import android.bluetooth.BluetoothDevice;
import android.media.MediaDescription;
import android.media.browse.MediaBrowser;
import android.media.browse.MediaBrowser.MediaItem;
import android.os.Bundle;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

// Browsing hierarchy.
// Root:
//      Player1:
//        Now_Playing:
//          MediaItem1
//          MediaItem2
//        Folder1
//        Folder2
//        ....
//      Player2
//      ....
public class BrowseTree {
    private static final String TAG = "BrowseTree";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    private static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    public static final String ROOT = "__ROOT__";
    public static final String UP = "__UP__";
    public static final String NOW_PLAYING_PREFIX = "NOW_PLAYING";
    public static final String PLAYER_PREFIX = "PLAYER";

    // Static instance of Folder ID <-> Folder Instance (for navigation purposes)
    private final HashMap<String, BrowseNode> mBrowseMap = new HashMap<String, BrowseNode>();
    private BrowseNode mCurrentBrowseNode;
    private BrowseNode mCurrentBrowsedPlayer;
    private BrowseNode mCurrentAddressedPlayer;
    private int mDepth = 0;
    final BrowseNode mRootNode;
    final BrowseNode mNavigateUpNode;
    final BrowseNode mNowPlayingNode;

    BrowseTree(BluetoothDevice device) {
        if (device == null) {
            mRootNode = new BrowseNode(new MediaItem(new MediaDescription.Builder()
                    .setMediaId(ROOT).setTitle(ROOT).build(), MediaItem.FLAG_BROWSABLE));
            mRootNode.setCached(true);
        } else {
            mRootNode = new BrowseNode(new MediaItem(new MediaDescription.Builder()
                    .setMediaId(ROOT + device.getAddress().toString()).setTitle(
                            device.getName()).build(), MediaItem.FLAG_BROWSABLE));
            mRootNode.mDevice = device;

        }
        mRootNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST;
        mRootNode.setExpectedChildren(255);

        mNavigateUpNode = new BrowseNode(new MediaItem(new MediaDescription.Builder()
                .setMediaId(UP).setTitle(UP).build(),
                MediaItem.FLAG_BROWSABLE));

        mNowPlayingNode = new BrowseNode(new MediaItem(new MediaDescription.Builder()
                .setMediaId(NOW_PLAYING_PREFIX)
                .setTitle(NOW_PLAYING_PREFIX).build(), MediaItem.FLAG_BROWSABLE));
        mNowPlayingNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING;
        mNowPlayingNode.setExpectedChildren(255);
        mBrowseMap.put(ROOT, mRootNode);
        mBrowseMap.put(NOW_PLAYING_PREFIX, mNowPlayingNode);

        mCurrentBrowseNode = mRootNode;
    }

    public void clear() {
        // Clearing the map should garbage collect everything.
        mBrowseMap.clear();
    }

    void onConnected(BluetoothDevice device) {
        BrowseNode browseNode = new BrowseNode(device);
        mRootNode.addChild(browseNode);
    }

    BrowseNode getTrackFromNowPlayingList(int trackNumber) {
        return mNowPlayingNode.mChildren.get(trackNumber);
    }

    // Each node of the tree is represented by Folder ID, Folder Name and the children.
    class BrowseNode {
        // MediaItem to store the media related details.
        MediaItem mItem;

        BluetoothDevice mDevice;
        long mBluetoothId;

        // Type of this browse node.
        // Since Media APIs do not define the player separately we define that
        // distinction here.
        boolean mIsPlayer = false;

        // If this folder is currently cached, can be useful to return the contents
        // without doing another fetch.
        boolean mCached = false;

        byte mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_VFS;

        // List of children.
        private BrowseNode mParent;
        private final List<BrowseNode> mChildren = new ArrayList<BrowseNode>();
        private int mExpectedChildrenCount;

        BrowseNode(MediaItem item) {
            mItem = item;
            Bundle extras = mItem.getDescription().getExtras();
            if (extras != null) {
                mBluetoothId = extras.getLong(AvrcpControllerService.MEDIA_ITEM_UID_KEY);
            }
        }

        BrowseNode(AvrcpPlayer player) {
            mIsPlayer = true;

            // Transform the player into a item.
            MediaDescription.Builder mdb = new MediaDescription.Builder();
            String playerKey = PLAYER_PREFIX + player.getId();
            mBluetoothId = player.getId();

            mdb.setMediaId(UUID.randomUUID().toString());
            mdb.setTitle(player.getName());
            int mediaItemFlags = player.supportsFeature(AvrcpPlayer.FEATURE_BROWSING)
                    ? MediaBrowser.MediaItem.FLAG_BROWSABLE : 0;
            mItem = new MediaBrowser.MediaItem(mdb.build(), mediaItemFlags);
        }

        BrowseNode(BluetoothDevice device) {
            boolean mIsPlayer = true;
            mDevice = device;
            MediaDescription.Builder mdb = new MediaDescription.Builder();
            String playerKey = PLAYER_PREFIX + device.getAddress().toString();
            mdb.setMediaId(playerKey);
            mdb.setTitle(device.getName());
            int mediaItemFlags = MediaBrowser.MediaItem.FLAG_BROWSABLE;
            mItem = new MediaBrowser.MediaItem(mdb.build(), mediaItemFlags);
        }

        private BrowseNode(String name) {
            MediaDescription.Builder mdb = new MediaDescription.Builder();
            mdb.setMediaId(name);
            mdb.setTitle(name);
            mItem = new MediaBrowser.MediaItem(mdb.build(), MediaBrowser.MediaItem.FLAG_BROWSABLE);
        }

        synchronized void setExpectedChildren(int count) {
            mExpectedChildrenCount = count;
        }

        synchronized int getExpectedChildren() {
            return mExpectedChildrenCount;
        }

        synchronized <E> int addChildren(List<E> newChildren) {
            for (E child : newChildren) {
                BrowseNode currentNode = null;
                if (child instanceof MediaItem) {
                    currentNode = new BrowseNode((MediaItem) child);
                } else if (child instanceof AvrcpPlayer) {
                    currentNode = new BrowseNode((AvrcpPlayer) child);
                }
                addChild(currentNode);
            }
            return newChildren.size();
        }

        synchronized boolean addChild(BrowseNode node) {
            if (node != null) {
                node.mParent = this;
                if (this.mBrowseScope == AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING) {
                    node.mBrowseScope = this.mBrowseScope;
                }
                if (node.mDevice == null) {
                    node.mDevice = this.mDevice;
                }
                mChildren.add(node);
                mBrowseMap.put(node.getID(), node);
                return true;
            }
            return false;
        }

        synchronized void removeChild(BrowseNode node) {
            mChildren.remove(node);
            mBrowseMap.remove(node.getID());
        }

        synchronized int getChildrenCount() {
            return mChildren.size();
        }

        synchronized List<BrowseNode> getChildren() {
            return mChildren;
        }

        synchronized BrowseNode getParent() {
            return mParent;
        }

        synchronized List<MediaItem> getContents() {
            if (mChildren.size() > 0 || mCached) {
                List<MediaItem> contents = new ArrayList<MediaItem>(mChildren.size());
                for (BrowseNode child : mChildren) {
                    contents.add(child.getMediaItem());
                }
                return contents;
            }
            return null;
        }

        synchronized boolean isChild(BrowseNode node) {
            return mChildren.contains(node);
        }

        synchronized boolean isCached() {
            return mCached;
        }

        synchronized boolean isBrowsable() {
            return mItem.isBrowsable();
        }

        synchronized void setCached(boolean cached) {
            if (DBG) Log.d(TAG, "Set Cache" + cached + "Node" + toString());
            mCached = cached;
            if (!cached) {
                for (BrowseNode child : mChildren) {
                    mBrowseMap.remove(child.getID());
                }
                mChildren.clear();
            }
        }

        // Fetch the Unique UID for this item, this is unique across all elements in the tree.
        synchronized String getID() {
            return mItem.getDescription().getMediaId();
        }

        // Get the BT Player ID associated with this node.
        synchronized int getPlayerID() {
            return Integer.parseInt(getID().replace(PLAYER_PREFIX, ""));
        }

        synchronized byte getScope() {
            return mBrowseScope;
        }

        // Fetch the Folder UID that can be used to fetch folder listing via bluetooth.
        // This may not be unique hence this combined with direction will define the
        // browsing here.
        synchronized String getFolderUID() {
            return getID();
        }

        synchronized long getBluetoothID() {
            return mBluetoothId;
        }

        synchronized MediaItem getMediaItem() {
            return mItem;
        }

        synchronized boolean isPlayer() {
            return mIsPlayer;
        }

        synchronized boolean isNowPlaying() {
            return getID().startsWith(NOW_PLAYING_PREFIX);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof BrowseNode)) {
                return false;
            }
            BrowseNode otherNode = (BrowseNode) other;
            return getID().equals(otherNode.getID());
        }

        @Override
        public synchronized String toString() {
            if (VDBG) {
                String serialized = "[ Name: " + mItem.getDescription().getTitle()
                        + " Scope:" + mBrowseScope + " expected Children: "
                        + mExpectedChildrenCount + "] ";
                for (BrowseNode node : mChildren) {
                    serialized += node.toString();
                }
                return serialized;
            } else {
                return "ID: " + getID();
            }
        }

        // Returns true if target is a descendant of this.
        synchronized boolean isDescendant(BrowseNode target) {
            return getEldestChild(this, target) == null ? false : true;
        }
    }

    synchronized BrowseNode findBrowseNodeByID(String parentID) {
        BrowseNode bn = mBrowseMap.get(parentID);
        if (bn == null) {
            Log.e(TAG, "folder " + parentID + " not found!");
            return null;
        }
        if (VDBG) {
            Log.d(TAG, "Size" + mBrowseMap.size());
        }
        return bn;
    }

    synchronized boolean setCurrentBrowsedFolder(String uid) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            Log.e(TAG, "Setting an unknown browsed folder, ignoring bn " + uid);
            return false;
        }

        // Set the previous folder as not cached so that we fetch the contents again.
        if (!bn.equals(mCurrentBrowseNode)) {
            Log.d(TAG, "Set cache  " + bn + " curr " + mCurrentBrowseNode);
        }
        mCurrentBrowseNode = bn;
        return true;
    }

    synchronized BrowseNode getCurrentBrowsedFolder() {
        return mCurrentBrowseNode;
    }

    synchronized boolean setCurrentBrowsedPlayer(String uid, int items, int depth) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            Log.e(TAG, "Setting an unknown browsed player, ignoring bn " + uid);
            return false;
        }
        mCurrentBrowsedPlayer = bn;
        mCurrentBrowseNode = mCurrentBrowsedPlayer;
        for (Integer level = 0; level < depth; level++) {
            BrowseNode dummyNode = new BrowseNode(level.toString());
            dummyNode.mParent = mCurrentBrowseNode;
            dummyNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_VFS;
            mCurrentBrowseNode = dummyNode;
        }
        mCurrentBrowseNode.setExpectedChildren(items);
        mDepth = depth;
        return true;
    }

    synchronized BrowseNode getCurrentBrowsedPlayer() {
        return mCurrentBrowsedPlayer;
    }

    synchronized boolean setCurrentAddressedPlayer(String uid) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            if (DBG) Log.d(TAG, "Setting an unknown addressed player, ignoring bn " + uid);
            mRootNode.setCached(false);
            mRootNode.mChildren.add(mNowPlayingNode);
            mBrowseMap.put(NOW_PLAYING_PREFIX, mNowPlayingNode);
            return false;
        }
        mCurrentAddressedPlayer = bn;
        return true;
    }

    synchronized BrowseNode getCurrentAddressedPlayer() {
        return mCurrentAddressedPlayer;
    }

    @Override
    public String toString() {
        String serialized = "Size: " + mBrowseMap.size();
        if (VDBG) {
            serialized += mRootNode.toString();
        }
        return serialized;
    }

    // Calculates the path to target node.
    // Returns: UP node to go up
    // Returns: target node if there
    // Returns: named node to go down
    // Returns: null node if unknown
    BrowseNode getNextStepToFolder(BrowseNode target) {
        if (target == null) {
            return null;
        } else if (target.equals(mCurrentBrowseNode)
                || target.equals(mNowPlayingNode)
                || target.equals(mRootNode)) {
            return target;
        } else if (target.isPlayer()) {
            if (mDepth > 0) {
                mDepth--;
                return mNavigateUpNode;
            } else {
                return target;
            }
        } else if (mBrowseMap.get(target.getID()) == null) {
            return null;
        } else {
            BrowseNode nextChild = getEldestChild(mCurrentBrowseNode, target);
            if (nextChild == null) {
                return mNavigateUpNode;
            } else {
                return nextChild;
            }
        }
    }

    static BrowseNode getEldestChild(BrowseNode ancestor, BrowseNode target) {
        // ancestor is an ancestor of target
        BrowseNode descendant = target;
        if (DBG) {
            Log.d(TAG, "NAVIGATING ancestor" + ancestor.toString() + "Target"
                    + target.toString());
        }
        while (!ancestor.equals(descendant.mParent)) {
            descendant = descendant.mParent;
            if (descendant == null) {
                return null;
            }
        }
        if (DBG) Log.d(TAG, "NAVIGATING Descendant" + descendant.toString());
        return descendant;
    }
}
