/******************************************************************************
 *  Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *      * Neither the name of The Linux Foundation nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

package com.android.bluetooth.groupclient;

import android.bluetooth.IBluetoothGroupCallback;
import android.bluetooth.BluetoothGroupCallback;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.RemoteException;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Collections;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.UUID;

/* This class keeps track of registered GroupClient applications and
 * managing callbacks to be given to appropriate app or module */

public class GroupAppMap {

    private static final String TAG = "BluetoothGroupAppMap";

    class GroupClientApp {
        /* The UUID of the application */
        public UUID uuid;

        /* The id of the application */
        public int appId;

        /* flag to determine if Bluetooth module has registered. */
        public boolean isLocal;

        /* Callbacks to be given to application */
        public IBluetoothGroupCallback appCb;

        /* Callbacks to be given to registered Bluetooth modules*/
        public BluetoothGroupCallback  mCallback;

        public boolean isRegistered;

        /** Death receipient */
        private IBinder.DeathRecipient mDeathRecipient;

        GroupClientApp(UUID uuid, boolean isLocal, IBluetoothGroupCallback appCb,
                BluetoothGroupCallback  localCallbacks) {
            this.uuid = uuid;
            this.isLocal = isLocal;
            this.appCb = appCb;
            this.mCallback = localCallbacks;
            this.isRegistered = true;
            appUuids.add(uuid);
        }

        /**
         * To link death recipient
         */
        void linkToDeath(IBinder.DeathRecipient deathRecipient) {
            try {
                IBinder binder = ((IInterface) appCb).asBinder();
                binder.linkToDeath(deathRecipient, 0);
                mDeathRecipient = deathRecipient;
            } catch (RemoteException e) {
                Log.e(TAG, "Unable to link deathRecipient for appId: " + appId);
            }
        }

    }

    List<GroupClientApp> mApps = Collections.synchronizedList(new ArrayList<GroupClientApp>());

    ArrayList<UUID> appUuids = new ArrayList<UUID>();

    /**
     * Add an entry to the application list.
     */
    GroupClientApp add(UUID uuid, boolean isLocal, IBluetoothGroupCallback appCb,
            BluetoothGroupCallback  localCallback) {
        synchronized (mApps) {
            GroupClientApp app = new GroupClientApp(uuid, isLocal, appCb, localCallback);
            mApps.add(app);
            return app;
        }
    }

    /**
     * Remove the entry for a given UUID
     */
    void remove(UUID uuid) {
        synchronized (mApps) {
            Iterator<GroupClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                GroupClientApp entry = i.next();
                if (entry.uuid.equals(uuid)) {
                    entry.isRegistered = false;
                    i.remove();
                    break;
                }
            }
        }
    }

    /**
     * Remove the entry for a given application ID.
     */
    void remove(int appId) {
        synchronized (mApps) {
            Iterator<GroupClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                GroupClientApp entry = i.next();
                if (entry.appId == appId) {
                    entry.isRegistered = false;
                    i.remove();
                    break;
                }
            }
        }
    }

    /**
     * Get GroupClient application by UUID.
     */
    GroupClientApp getByUuid(UUID uuid) {
        synchronized (mApps) {
            Iterator<GroupClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                GroupClientApp entry = i.next();
                if (entry.uuid.equals(uuid)) {
                    return entry;
                }
            }
        }
        Log.e(TAG, "App not found for UUID " + uuid);
        return null;
    }

    /**
     * Get a GroupClient application by appId.
     */
    GroupClientApp getById(int appId) {
        synchronized (mApps) {
            Iterator<GroupClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                GroupClientApp entry = i.next();
                if (entry.appId == appId) {
                    return entry;
                }
            }
        }
        Log.e(TAG, "GroupClient App not found for appId " + appId);
        return null;
    }
}
