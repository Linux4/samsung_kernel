/*
 * Copyright 2019 The Android Open Source Project
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

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.UserHandle;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.internal.annotations.VisibleForTesting;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The silence device manager controls silence mode for A2DP, HFP, and AVRCP.
 *
 * 1) If an active device (for A2DP or HFP) enters silence mode, the active device
 *    for that profile will be set to null.
 * 2) If a device exits silence mode while the A2DP or HFP active device is null,
 *    the device will be set as the active device for that profile.
 * 3) If a device is disconnected, it exits silence mode.
 * 4) If a device is set as the active device for A2DP or HFP, while silence mode
 *    is enabled, then the device will exit silence mode.
 * 5) If a device is in silence mode, AVRCP position change event and HFP AG indicators
 *    will be disabled.
 * 6) If a device is not connected with A2DP or HFP, it cannot enter silence mode.
 */
public class SilenceDeviceManager {
    private static final boolean DBG = true;
    private static final boolean VERBOSE = false;
    private static final String TAG = "SilenceDeviceManager";

    private final AdapterService mAdapterService;
    private final ServiceFactory mFactory;
    private Handler mHandler = null;
    private Looper mLooper = null;

    private final Map<BluetoothDevice, Boolean> mSilenceDevices = new HashMap<>();
    private final List<BluetoothDevice> mA2dpConnectedDevices = new ArrayList<>();
    private final List<BluetoothDevice> mHfpConnectedDevices = new ArrayList<>();

    private static final int MSG_SILENCE_DEVICE_STATE_CHANGED = 1;
    private static final int MSG_A2DP_CONNECTION_STATE_CHANGED = 10;
    private static final int MSG_HFP_CONNECTION_STATE_CHANGED = 11;
    private static final int MSG_A2DP_ACTIVE_DEIVCE_CHANGED = 20;
    private static final int MSG_HFP_ACTIVE_DEVICE_CHANGED = 21;
    private static final int ENABLE_SILENCE = 0;
    private static final int DISABLE_SILENCE = 1;

    // Broadcast receiver for all changes
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.e(TAG, "Received intent with null action");
                return;
            }
            switch (action) {
                case BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MSG_A2DP_CONNECTION_STATE_CHANGED,
                                           intent).sendToTarget();
                    break;
                case BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MSG_HFP_CONNECTION_STATE_CHANGED,
                                           intent).sendToTarget();
                    break;
                case BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MSG_A2DP_ACTIVE_DEIVCE_CHANGED,
                                           intent).sendToTarget();
                    break;
                case BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MSG_HFP_ACTIVE_DEVICE_CHANGED,
                        intent).sendToTarget();
                    break;
                default:
                    Log.e(TAG, "Received unexpected intent, action=" + action);
                    break;
            }
        }
    };

    class SilenceDeviceManagerHandler extends Handler {
        SilenceDeviceManagerHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (VERBOSE) {
                Log.d(TAG, "handleMessage: " + msg.what);
            }
            switch (msg.what) {
                case MSG_SILENCE_DEVICE_STATE_CHANGED: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    boolean state = (msg.arg1 == ENABLE_SILENCE);
                    handleSilenceDeviceStateChanged(device, state);
                }
                break;

                case MSG_A2DP_CONNECTION_STATE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    int prevState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
                    int nextState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);

                    if (nextState == BluetoothProfile.STATE_CONNECTED) {
                        // enter connected state
                        addConnectedDevice(device, BluetoothProfile.A2DP);
                        if (!mSilenceDevices.containsKey(device)) {
                            mSilenceDevices.put(device, false);
                        }
                    } else if (prevState == BluetoothProfile.STATE_CONNECTED) {
                        // exiting from connected state
                        removeConnectedDevice(device, BluetoothProfile.A2DP);
                        if (!isBluetoothAudioConnected(device)) {
                            handleSilenceDeviceStateChanged(device, false);
                            mSilenceDevices.remove(device);
                        }
                    }
                }
                break;

                case MSG_HFP_CONNECTION_STATE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    int prevState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
                    int nextState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);

                    if (nextState == BluetoothProfile.STATE_CONNECTED) {
                        // enter connected state
                        addConnectedDevice(device, BluetoothProfile.HEADSET);
                        if (!mSilenceDevices.containsKey(device)) {
                            mSilenceDevices.put(device, false);
                        }
                    } else if (prevState == BluetoothProfile.STATE_CONNECTED) {
                        // exiting from connected state
                        removeConnectedDevice(device, BluetoothProfile.HEADSET);
                        if (!isBluetoothAudioConnected(device)) {
                            handleSilenceDeviceStateChanged(device, false);
                            mSilenceDevices.remove(device);
                        }
                    }
                }
                break;

                case MSG_A2DP_ACTIVE_DEIVCE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice a2dpActiveDevice =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    if (getSilenceMode(a2dpActiveDevice)) {
                        // Resume the device from silence mode.
                        setSilenceMode(a2dpActiveDevice, false);
                    }
                }
                break;

                case MSG_HFP_ACTIVE_DEVICE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice hfpActiveDevice =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    if (getSilenceMode(hfpActiveDevice)) {
                        // Resume the device from silence mode.
                        setSilenceMode(hfpActiveDevice, false);
                    }
                }
                break;

                default: {
                    Log.e(TAG, "Unknown message: " + msg.what);
                }
                break;
            }
        }
    };

    SilenceDeviceManager(AdapterService service, ServiceFactory factory, Looper looper) {
        mAdapterService = service;
        mFactory = factory;
        mLooper = looper;
    }

    void start() {
        if (VERBOSE) {
            Log.v(TAG, "start()");
        }
        mHandler = new SilenceDeviceManagerHandler(mLooper);
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED);
        filter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED);
        mAdapterService.registerReceiver(mReceiver, filter);
    }

    void cleanup() {
        if (VERBOSE) {
            Log.v(TAG, "cleanup()");
        }
        mSilenceDevices.clear();
        mAdapterService.unregisterReceiver(mReceiver);
    }

    @VisibleForTesting
    boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        if (mHandler == null) {
            Log.e(TAG, "setSilenceMode() mHandler is null!");
            return false;
        }
        Log.d(TAG, "setSilenceMode: " + device.getAddress() + ", " + silence);
        Message message = mHandler.obtainMessage(MSG_SILENCE_DEVICE_STATE_CHANGED,
                silence ? ENABLE_SILENCE : DISABLE_SILENCE, 0, device);
        mHandler.sendMessage(message);
        return true;
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.MODIFY_PHONE_STATE,
    })
    void handleSilenceDeviceStateChanged(BluetoothDevice device, boolean state) {
        boolean oldState = getSilenceMode(device);
        if (oldState == state) {
            return;
        }
        if (!isBluetoothAudioConnected(device)) {
            if (oldState) {
                // Device is disconnected, resume all silenced profiles.
                state = false;
            } else {
                Log.d(TAG, "Deivce is not connected to any Bluetooth audio.");
                return;
            }
        }
        mSilenceDevices.replace(device, state);

        A2dpService a2dpService = mFactory.getA2dpService();
        if (a2dpService != null) {
            a2dpService.setSilenceMode(device, state);
        }
        HeadsetService headsetService = mFactory.getHeadsetService();
        if (headsetService != null) {
            headsetService.setSilenceMode(device, state);
        }
        Log.i(TAG, "Silence mode change " + device.getAddress() + ": " + oldState + " -> "
                + state);
        broadcastSilenceStateChange(device, state);
    }

    void broadcastSilenceStateChange(BluetoothDevice device, boolean state) {
        Intent intent = new Intent(BluetoothDevice.ACTION_SILENCE_MODE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        mAdapterService.sendBroadcastAsUser(intent, UserHandle.ALL, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());

    }

    @VisibleForTesting
    boolean getSilenceMode(BluetoothDevice device) {
        boolean state = false;
        if (mSilenceDevices.containsKey(device)) {
            state = mSilenceDevices.get(device);
        }
        return state;
    }

    void addConnectedDevice(BluetoothDevice device, int profile) {
        if (VERBOSE) {
            Log.d(TAG, "addConnectedDevice: " + device.getAddress() + ", profile:" + profile);
        }
        switch (profile) {
            case BluetoothProfile.A2DP:
                if (!mA2dpConnectedDevices.contains(device)) {
                    mA2dpConnectedDevices.add(device);
                }
                break;
            case BluetoothProfile.HEADSET:
                if (!mHfpConnectedDevices.contains(device)) {
                    mHfpConnectedDevices.add(device);
                }
                break;
        }
    }

    void removeConnectedDevice(BluetoothDevice device, int profile) {
        if (VERBOSE) {
            Log.d(TAG, "removeConnectedDevice: " + device.getAddress() + ", profile:" + profile);
        }
        switch (profile) {
            case BluetoothProfile.A2DP:
                if (mA2dpConnectedDevices.contains(device)) {
                    mA2dpConnectedDevices.remove(device);
                }
                break;
            case BluetoothProfile.HEADSET:
                if (mHfpConnectedDevices.contains(device)) {
                    mHfpConnectedDevices.remove(device);
                }
                break;
        }
    }

    boolean isBluetoothAudioConnected(BluetoothDevice device) {
        return (mA2dpConnectedDevices.contains(device) || mHfpConnectedDevices.contains(device));
    }

    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        writer.println("\nSilenceDeviceManager:");
        writer.println("  Address            | Is silenced?");
        for (BluetoothDevice device : mSilenceDevices.keySet()) {
            writer.println("  " + device.getAddress() + "  | " + getSilenceMode(device));
        }
    }

    @VisibleForTesting
    BroadcastReceiver getBroadcastReceiver() {
        return mReceiver;
    }
}
