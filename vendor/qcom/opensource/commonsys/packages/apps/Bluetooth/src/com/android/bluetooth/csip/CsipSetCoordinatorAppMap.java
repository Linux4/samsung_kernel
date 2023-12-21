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

package com.android.bluetooth.csip;

import android.bluetooth.IBluetoothGroupCallback;
import android.bluetooth.BluetoothGroupCallback;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;

import android.util.Log;

/* This class keeps track of registered GroupClient applications and
 * managing callbacks to be given to appropriate app or module */

public class CsipSetCoordinatorAppMap {

    private static final String TAG = "CsipSetCoordinatorAppMap";

    class CsipSetCoordinatorClientApp {
        /* The UUID of the application */
        public UUID uuid;

        /* The id of the application */
        public int appId;

        /* flag to determine if Bluetooth module has registered. */
        public boolean isLocal;

         /* Call backs to be given to registered Bluetooth modules*/
        public BluetoothGroupCallback  mCallback;

        public boolean isRegistered;

        CsipSetCoordinatorClientApp(UUID uuid, boolean isLocal,
                BluetoothGroupCallback  localCallbacks) {
            this.uuid = uuid;
            this.isLocal = isLocal;
            this.mCallback = localCallbacks;
            this.isRegistered = true;
            appUuids.add(uuid);
        }

    }

    List<CsipSetCoordinatorClientApp> mApps
            = Collections.synchronizedList(new ArrayList<CsipSetCoordinatorClientApp>());

    ArrayList<UUID> appUuids = new ArrayList<UUID>();

    /**
     * Add an entry to the application list.
     */
    CsipSetCoordinatorClientApp add(UUID uuid, boolean isLocal,
                BluetoothGroupCallback  localCallback) {
        synchronized (mApps) {
            CsipSetCoordinatorClientApp app
                    = new CsipSetCoordinatorClientApp(uuid, isLocal, localCallback);
            mApps.add(app);
            return app;
        }
    }

    /**
     * Remove the entry for a given UUID
     */
    void remove(UUID uuid) {
        synchronized (mApps) {
            Iterator<CsipSetCoordinatorClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                CsipSetCoordinatorClientApp entry = i.next();
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
            Iterator<CsipSetCoordinatorClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                CsipSetCoordinatorClientApp entry = i.next();
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
    CsipSetCoordinatorClientApp getByUuid(UUID uuid) {
        synchronized (mApps) {
            Iterator<CsipSetCoordinatorClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                CsipSetCoordinatorClientApp entry = i.next();
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
    CsipSetCoordinatorClientApp getById(int appId) {
        synchronized (mApps) {
            Iterator<CsipSetCoordinatorClientApp> i = mApps.iterator();
            while (i.hasNext()) {
                CsipSetCoordinatorClientApp entry = i.next();
                if (entry.appId == appId) {
                    return entry;
                }
            }
        }
        Log.e(TAG, "GroupClient App not found for appId " + appId);
        return null;
    }
}
