/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.bluetooth.btservice;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.Utils;

/**
 * Base class for a background service that runs a Bluetooth profile
 */
public abstract class ProfileService extends Service {
    private static final boolean DBG = false;

    public static final String BLUETOOTH_PRIVILEGED =
            android.Manifest.permission.BLUETOOTH_PRIVILEGED;

    public interface IProfileServiceBinder extends IBinder {
        /**
         * Called in {@link #onDestroy()}
         */
        void cleanup();
    }

    //Profile services will not be automatically restarted.
    //They must be explicitly restarted by AdapterService
    private static final int PROFILE_SERVICE_MODE = Service.START_NOT_STICKY;
    protected BluetoothAdapter mAdapter;
    private IProfileServiceBinder mBinder;
    private final String mName;
    private AdapterService mAdapterService;
    private BroadcastReceiver mUserSwitchedReceiver;
    private boolean mProfileStarted = false;
    private volatile boolean mTestModeEnabled = false;

    public String getName() {
        return getClass().getSimpleName();
    }

    public boolean isAvailable() {
        return mProfileStarted;
    }

    protected boolean isTestModeEnabled() {
        return mTestModeEnabled;
    }

    /**
     * Called in {@link #onCreate()} to init binder interface for this profile service
     *
     * @return initialized binder interface for this profile service
     */
    protected abstract IProfileServiceBinder initBinder();

    /**
     * Called in {@link #onCreate()} to init basic stuff in this service
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected void create() {}

    /**
     * Called in {@link #onStartCommand(Intent, int, int)} when the service is started by intent
     *
     * @return True in successful condition, False otherwise
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected abstract boolean start();

    /**
     * Called in {@link #onStartCommand(Intent, int, int)} when the service is stopped by intent
     *
     * @return True in successful condition, False otherwise
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected abstract boolean stop();

    /**
     * Called in {@link #onDestroy()} when this object is completely discarded
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected void cleanup() {}

    /**
     * @param userId is equivalent to the result of ActivityManager.getCurrentUser()
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected void setCurrentUser(int userId) {}

    /**
     * @param userId is equivalent to the result of ActivityManager.getCurrentUser()
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected void setUserUnlocked(int userId) {}

    /**
     * @param testEnabled if the profile should enter or exit a testing mode
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    protected void setTestModeEnabled(boolean testModeEnabled) {
        mTestModeEnabled = testModeEnabled;
    }

    protected ProfileService() {
        mName = getName();
    }

    @Override
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public void onCreate() {
        if (DBG) {
            Log.d(mName, "onCreate");
        }
        super.onCreate();
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mBinder = initBinder();
        create();
    }

    @Override
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DBG) {
            Log.d(mName, "onStartCommand()");
        }
        AdapterService adapterService = AdapterService.getAdapterService();
        if (checkCallingOrSelfPermission(BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
            Log.e(mName, "Permission denied!");
            return PROFILE_SERVICE_MODE;
        }

        if (intent == null) {
            Log.d(mName, "onStartCommand ignoring null intent.");
            return PROFILE_SERVICE_MODE;
        }

        String action = intent.getStringExtra(AdapterService.EXTRA_ACTION);
        if (AdapterService.ACTION_SERVICE_STATE_CHANGED.equals(action)) {
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
            int currentState = (adapterService != null) ? adapterService.getState() : -1;
            if (state == BluetoothAdapter.STATE_OFF) {
                if ((currentState == BluetoothAdapter.STATE_TURNING_OFF &&
                     !mName.equals("GattService")) ||
                     (currentState == BluetoothAdapter.STATE_BLE_TURNING_OFF &&
                     mName.equals("GattService")) ) {
                    Log.d(mName, ": Received stop request...Stopping profile...");
                    doStop();
                } else {
                    Log.e(mName, ":intent received late, not Stopping profile");
                }
            } else if (state == BluetoothAdapter.STATE_ON) {
                if ((currentState == BluetoothAdapter.STATE_TURNING_ON &&
                     !mName.equals("GattService")) ||
                     (currentState == BluetoothAdapter.STATE_BLE_TURNING_ON &&
                     mName.equals("GattService")) ) {

                     Log.d(mName, "Received start request. Starting profile...");
                     doStart();
                } else {
                     Log.e(mName, ":intent received late, not starting profile");
                     if (adapterService != null) {
                         adapterService.removeProfile(this);
                     }
                }
            }
        }
        return PROFILE_SERVICE_MODE;
    }

    @Override
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public IBinder onBind(Intent intent) {
        if (DBG) {
            Log.d(mName, "onBind");
        }
        if (mAdapter != null && mBinder == null) {
            // initBinder returned null, you can't bind
            throw new UnsupportedOperationException("Cannot bind to " + mName);
        }
        return mBinder;
    }

    @Override
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public boolean onUnbind(Intent intent) {
        if (DBG) {
            Log.d(mName, "onUnbind");
        }
        return super.onUnbind(intent);
    }

    /**
     * Set the availability of an owned/managed component (Service, Activity, Provider, etc.)
     * using a string class name assumed to be in the Bluetooth package.
     *
     * It's expected that profiles can have a set of components that they may use to provide
     * features or interact with other services/the user. Profiles are expected to enable those
     * components when they start, and disable them when they stop.
     *
     * @param className The class name of the owned component residing in the Bluetooth package
     * @param enable True to enable the component, False to disable it
     */
    protected void setComponentAvailable(String className, boolean enable) {
        if (DBG) {
            Log.d(mName, "setComponentAvailable(className=" + className + ", enable=" + enable
                    + ")");
        }
        if (className == null) {
            return;
        }
        ComponentName component = new ComponentName(getPackageName(), className);
        setComponentAvailable(component, enable);
    }

    /**
     * Set the availability of an owned/managed component (Service, Activity, Provider, etc.)
     *
     * It's expected that profiles can have a set of components that they may use to provide
     * features or interact with other services/the user. Profiles are expected to enable those
     * components when they start, and disable them when they stop.
     *
     * @param component The component name of owned component
     * @param enable True to enable the component, False to disable it
     */
    protected void setComponentAvailable(ComponentName component, boolean enable) {
        if (DBG) {
            Log.d(mName, "setComponentAvailable(component=" + component + ", enable=" + enable
                    + ")");
        }
        if (component == null) {
            return;
        }
        getPackageManager().setComponentEnabledSetting(
                component,
                enable ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                       : PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP | PackageManager.SYNCHRONOUS);
    }

    /**
     * Support dumping profile-specific information for dumpsys
     *
     * @param sb StringBuilder from the profile.
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public void dump(StringBuilder sb) {
        sb.append("\nProfile: ");
        sb.append(mName);
        sb.append("\n");
    }

    /**
     * Support dumping scan events from GattService
     *
     * @param builder metrics proto builder
     */
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public void dumpProto(BluetoothMetricsProto.BluetoothLog.Builder builder) {
        // Do nothing
    }

    /**
     * Append an indented String for adding dumpsys support to subclasses.
     *
     * @param sb StringBuilder from the profile.
     * @param s String to indent and append.
     */
    public static void println(StringBuilder sb, String s) {
        sb.append("  ");
        sb.append(s);
        sb.append("\n");
    }

    @Override
    // Suppressed since this is called from framework
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public void onDestroy() {
        cleanup();
        if (mBinder != null) {
            mBinder.cleanup();
            mBinder = null;
        }
        mAdapter = null;
        super.onDestroy();
    }

    private void doStart() {
        if (mAdapter == null) {
            Log.w(mName, "Can't start profile service: device does not have BT");
            return;
        }

        mAdapterService = AdapterService.getAdapterService();
        if (mAdapterService == null) {
            Log.w(mName, "Could not add this profile because AdapterService is null.");
            return;
        }
        mAdapterService.addProfile(this);

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_USER_SWITCHED);
        filter.addAction(Intent.ACTION_USER_UNLOCKED);
        mUserSwitchedReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final String action = intent.getAction();
                final int userId =
                        intent.getIntExtra(Intent.EXTRA_USER_HANDLE, UserHandle.USER_NULL);
                if (userId == UserHandle.USER_NULL) {
                    Log.e(mName, "userChangeReceiver received an invalid EXTRA_USER_HANDLE");
                    return;
                }
                if (Intent.ACTION_USER_SWITCHED.equals(action)) {
                    Log.d(mName, "User switched to userId " + userId);
                    setCurrentUser(userId);
                } else if (Intent.ACTION_USER_UNLOCKED.equals(intent.getAction())) {
                    Log.d(mName, "Unlocked userId " + userId);
                    setUserUnlocked(userId);
                }
            }
        };

        getApplicationContext().registerReceiver(mUserSwitchedReceiver, filter);
        int currentUserId = ActivityManager.getCurrentUser();
        setCurrentUser(currentUserId);
        UserManager userManager = UserManager.get(getApplicationContext());
        if (userManager.isUserUnlocked(currentUserId)) {
            setUserUnlocked(currentUserId);
        }
        mProfileStarted = start();
        if (!mProfileStarted) {
            Log.e(mName, "Error starting profile. start() returned false.");
            return;
        }
        Log.d(mName, " profile started successfully");
        mAdapterService.onProfileServiceStateChanged(this, BluetoothAdapter.STATE_ON);
    }

    private void doStop() {
        if (!mProfileStarted) {
            Log.w(mName, "doStop() called, but the profile is not running.");
        }
        mProfileStarted = false;
        if (!stop()) {
            Log.e(mName, "Unable to stop profile");
        } else {
            Log.d(mName, " profile stopped successfully");
        }
        if (mAdapterService != null) {
            mAdapterService.onProfileServiceStateChanged(this, BluetoothAdapter.STATE_OFF);
        }
        if (mAdapterService != null) {
            mAdapterService.removeProfile(this);
        }
        if (mUserSwitchedReceiver != null) {
            getApplicationContext().unregisterReceiver(mUserSwitchedReceiver);
            mUserSwitchedReceiver = null;
        }
        stopSelf();
    }

    protected BluetoothDevice getDevice(byte[] address) {
        if (mAdapter != null) {
            return mAdapter.getRemoteDevice(Utils.getAddressStringFromByte(address));
        }
        return null;
    }
}
