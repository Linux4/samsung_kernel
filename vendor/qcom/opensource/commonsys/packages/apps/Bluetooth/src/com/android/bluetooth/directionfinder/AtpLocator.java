/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

package com.android.bluetooth.directionfinder;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.SystemProperties;
import android.os.UserManager;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.ArrayUtils;

import com.android.bluetooth.directionfinder.AtpEnableDirFinding;

import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

public class AtpLocator {
    private static final String TAG = "AtpLocator";
    private static final boolean DBG = true;
    private static final int MAX_ATP_STATE_MACHINES = 10;
    private static final String ACTION_CONNECT_DEVICE =
                "com.android.bluetooth.atp.test.action.CONNECT_DEVICE";
    private static final String ACTION_DISCONNECT_DEVICE =
                "com.android.bluetooth.atp.test.action.DISCONNECT_DEVICE";

    private HandlerThread mStateMachinesThread;
    private final HashMap<BluetoothDevice, AtpLocatorStateMachine> mStateMachines =
                new HashMap<>();
    private HashMap<BluetoothDevice, Integer> mConnectionMode = new HashMap();
    private BroadcastReceiver mBondStateChangedReceiver;

    private AdapterService mAdapterService;
    private AtpLocatorService mAtpLocatorService;
    private AtpLocatorNativeInterface mNativeInterface;
    private static AtpLocator sInstance = null;
    private Context mContext;

    private AtpLocator(Context context) {
        if (DBG) {
            Log.d(TAG, "Create AtpLocator Instance");
        }

        mContext = context;
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when AtpLocator starts");
        mNativeInterface = Objects.requireNonNull(AtpLocatorNativeInterface.getInstance(),
                "AtpLocatorNativeInterface cannot be null when AtpLocator starts");

        // Start handler thread for state machines
        mStateMachines.clear();
        mStateMachinesThread = new HandlerThread("AtpLocator.StateMachines");
        mStateMachinesThread.start();
        Log.d(TAG, " AtpLocator: Calling Native Interface init");
        mNativeInterface.init();

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        mBondStateChangedReceiver = new BondStateChangedReceiver();
        mContext.registerReceiver(mBondStateChangedReceiver, filter);
    }

    /**
     * Make AtpLocator instance and Initialize
     *
     * @param context: application context
     * @return AtpLocator instance
     */
    public static AtpLocator make(Context context) {
        Log.d(TAG, "make");

        if(sInstance == null) {
            sInstance = new AtpLocator(context);
        }
        Log.v(TAG, "Exit make");
        return sInstance;
    }

    /**
     * Get the AtpLocator instance, which provides the public APIs
     * to ATP Locator operation via ATP connection
     *
     * @return AtpLocator instance
     */
    public static synchronized AtpLocator getAtpLocator() {
        if (sInstance == null) {
            Log.w(TAG, "getAtpLocator(): service is NULL");
            return null;
        }

        return sInstance;
    }

    /**
     * Clear AtpLocator instance
     *
     * @return void
     */
    public static void clearAtpInstance () {
        Log.v(TAG, "clearing ATP instatnce");
        sInstance = null;
        Log.v(TAG, "After clearing ATP instatnce ");
    }

    /**
     * Cleans up ATP profile interface and state machines
     *
     * @return void
     */
    public synchronized void doQuit() {
        if (DBG) {
            Log.d(TAG, "doQuit()");
        }
        if (sInstance == null) {
            Log.w(TAG, "doQuit() called before make()");
            return;
        }

        // Cleanup native interface
        mNativeInterface.cleanup();
        mNativeInterface = null;
        mContext.unregisterReceiver(mBondStateChangedReceiver);

        // Mark service as stopped
        sInstance = null;

        // Destroy state machines and stop handler thread
        synchronized (mStateMachines) {
            for (AtpLocatorStateMachine sm : mStateMachines.values()) {
                sm.doQuit();
                sm.cleanup();
            }
            mStateMachines.clear();
        }

        if (mStateMachinesThread != null) {
            mStateMachinesThread.quitSafely();
            mStateMachinesThread = null;
        }

        // Clear AdapterService
        mAdapterService = null;
    }

    /**
     * Establish connection to ATP profile
     *
     * @param device: BluetoothDevice to which ATP profile connection
     *                needs to be established
     * @return true, if connect request is sent to state machine successfully
     *         false, if connect request failed
     */
    public boolean connect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "connect(): " + device);
        }

        if (device == null) {
            return false;
        }

        synchronized (mStateMachines) {
            AtpLocatorStateMachine smConnect = getOrCreateStateMachine(device);
            if (smConnect == null) {
                Log.e(TAG, "Cannot connect to " + device + " : no state machine");
                return false;
            }

            if (smConnect.getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
                smConnect.sendMessage(AtpLocatorStateMachine.CONNECT, device);
            } else {
                Log.e(TAG, "Cannot connect to " + device + " : is already connected");
            }
        }
        return true;
    }

    /**
     * Disconnect ATP profile connection
     *
     * @param device: BluetoothDevice to which ATP profile connection
     *                needs to be disconnected
     * @return true, if disconnect request is sent to state machine successfully
     *         false, if disconnect request failed
     */
    public boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }

        if (device == null) {
            return false;
        }

        synchronized (mStateMachines) {
            AtpLocatorStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "disconnect: device " + device + " not ever connected/connecting");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "disconnect: device " + device
                        + " not connected/connecting, connectionState=" + connectionState);
                return false;
            }
            stateMachine.sendMessage(AtpLocatorStateMachine.DISCONNECT, device);
        }
        return true;
    }

    /**
     * Enable/Disable Ble Direction Finding to remote device
     *
     * @param device: remote device instance
     * @param samplingEnable: sampling Enable
     * @param slotDurations: slot Durations
     * @param enable: CTE enable
     * @param cteReqInt: CTE request interval
     * @param reqCteLen: Requested CTE length
     * @param reqCteType: Requested CTE Type
     * @return true if Enable/Disable of Ble Direction Finding
     * succeeded.
     */
    public boolean enableBleDirectionFinding(BluetoothDevice device, int samplingEnable,
            int slotDurations, int enable, int cteReqInt, int reqCteLen, int dirFindingType) {
        synchronized (mStateMachines) {
            Log.i(TAG, "enableBleDirectionFinding: device=" + device + ", " + Utils.getUidPidString());
            final AtpLocatorStateMachine stateMachine = mStateMachines.get(device);

            if (stateMachine == null) {
                Log.w(TAG, "enableBleDirectionFinding: device " + device + " was never connected/connecting");
                return false;
            }

            if (stateMachine.getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
                Log.w(TAG, "enableBleDirectionFinding: profile not connected");
                return false;
            }

            AtpEnableDirFinding atpEnableDfObj = new AtpEnableDirFinding(device, samplingEnable,
                    slotDurations, enable, cteReqInt, reqCteLen, dirFindingType);
            stateMachine.sendMessage(AtpLocatorStateMachine.ENABLE_BLE_DIRECTION_FINDING, atpEnableDfObj);
        }
        return true;
    }

    /**
     * Set ATP Locator Service instance
     *
     * @param atpService: AtpLocatorService instance which needs to be set
     * @return void
     */
    public void setAtpLocatorService(AtpLocatorService atpService) {
        Log.d(TAG, "setAtpLocatorSevice: " + atpService);
        mAtpLocatorService = atpService;
    }

    /**
     * Get ATP Locator Service instance
     *
     * @return AtpLocatorService instance
     */
    public AtpLocatorService getAtpLocatorService() {
        Log.d(TAG, "getAtpLocatorService: " + mAtpLocatorService);
        return mAtpLocatorService;
    }

    /**
     * Get Connected Devices
     *
     * @return List of connected devices
     */
    public List<BluetoothDevice> getConnectedDevices() {
        synchronized (mStateMachines) {
            List<BluetoothDevice> devices = new ArrayList<>();
            for (AtpLocatorStateMachine sm : mStateMachines.values()) {
                if (sm.isConnected()) {
                    devices.add(sm.getDevice());
                }
            }
            return devices;
        }
    }

    /**
     * Get devices matching connection states
     *
     * @param states array of states to match
     * @return List of connected devices which match the criteria
     */
    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            return devices;
        }
        final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        if (bondedDevices == null) {
            return devices;
        }
        synchronized (mStateMachines) {
            for (BluetoothDevice device : bondedDevices) {
                final ParcelUuid[] featureUuids = device.getUuids();
                if (!Utils.arrayContains(featureUuids, BluetoothUuid.CONSTANT_TONE_EXT_UUID)) {
                    continue;
                }
                int connectionState = BluetoothProfile.STATE_DISCONNECTED;
                AtpLocatorStateMachine sm = mStateMachines.get(device);
                if (sm != null) {
                    connectionState = sm.getConnectionState();
                }
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        break;
                    }
                }
            }
            return devices;
        }
    }

    /**
     * Get the current connection state of the ATP
     *
     * @param device is the remote bluetooth device
     * @return {@link BluetoothProfile#STATE_DISCONNECTED} if ATP is disconnected,
     * {@link BluetoothProfile#STATE_CONNECTING} if ATP is being connected,
     * {@link BluetoothProfile#STATE_CONNECTED} if ATP is connected, or
     * {@link BluetoothProfile#STATE_DISCONNECTING} if ATP is being disconnected
     */
    public int getConnectionState(BluetoothDevice device) {
        synchronized (mStateMachines) {
            AtpLocatorStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean okToConnect(BluetoothDevice device) {
        // Check if this is an incoming connection in Quiet mode.
        if (mAdapterService.isQuietModeEnabled()) {
            Log.e(TAG, "okToConnect: cannot connect to " + device + " : quiet mode enabled");
            return false;
        }

        int bondState = mAdapterService.getBondState(device);
        if (bondState != BluetoothDevice.BOND_BONDED) {
            Log.w(TAG, "okToConnect: return false, bondState=" + bondState);
            return false;
         }
        return true;
    }

    /**
     * Receives message from ATP profile native interface
     *
     * @param AtpStackEvent contains information from native interface
     * @return void
     */
    void messageFromNative(AtpStackEvent stackEvent) {
        Objects.requireNonNull(stackEvent.device,
                "Device should never be null, event: " + stackEvent);

        synchronized (mStateMachines) {
            BluetoothDevice device = stackEvent.device;
            AtpLocatorStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                if (stackEvent.type == AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                    switch (stackEvent.valueInt1) {
                        case AtpStackEvent.CONNECTION_STATE_CONNECTED:
                        case AtpStackEvent.CONNECTION_STATE_CONNECTING:
                            sm = getOrCreateStateMachine(device);
                            break;
                        default:
                            break;
                    }
                }
            }
            if (sm == null) {
                Log.e(TAG, "Cannot process stack event: no state machine: " + stackEvent);
                return;
            }
            sm.sendMessage(AtpLocatorStateMachine.STACK_EVENT, stackEvent);
        }
    }

    /**
     * Connecton State changed callback
     *
     * @param device is the remote device for ATP connection
     * @param newState new state of the connection
     * @param prevState previous state of the connection
     * @return void
     */
    void onConnectionStateChangedFromStateMachine(BluetoothDevice device,
            int newState, int prevState) {
        Log.d(TAG, "onConnectionStateChangedFromStateMachine for device: " + device
                    + " newState: " + newState);

        if (device == null) {
            Log.d(TAG, "device is null ");
            return;
        }
    }

    /**
     * Enable/Disable Ble Direction Finding callback
     *
     * @param device is the remote device for ATP connection
     * @param status of enable/disable Ble Direction Finding
     * @return void
     */
    void onEnableBleDirectionFinding(BluetoothDevice device, int status) {
        Log.d(TAG, "onEnableBleDirectionFinding for device: " + device + " status: " + status);

        if (mAtpLocatorService != null) {
            Log.d(TAG, "onEnableBleDirectionFinding:");
            mAtpLocatorService.onEnableBleDirectionFinding(device, status);
        }
    }

    /**
     * LE AoA results callback
     *
     * @param device is the remote device for ATP connection
     * @param status of LE AoA results received
     * @param azimuth value of node
     * @param azimuthUnc Uncertainty of azimuth result
     * @param elevation value fo node
     * @param elevationUnc Uncertainty of elevation result
     * @return void
     */
    void onLeAoaResults(BluetoothDevice device, int status, double azimuth, int azimuthUnc,
            double elevation, int elevationUnc) {
        Log.d(TAG, "onLeAoaResults for device: " + device + " status: " + status
              + " azimuth: " + azimuth + " elevation: " + elevation);

        if (mAtpLocatorService != null) {
            Log.d(TAG, "onLeAoaResults:");
            mAtpLocatorService.onLeAoaResults(device, status, azimuth, azimuthUnc,
                                              elevation, elevationUnc);
        }
    }

    private class BondStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) {
                return;
            }
            int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                           BluetoothDevice.ERROR);
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            Objects.requireNonNull(device, "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
            bondStateChanged(device, state);
        }
    }

    /**
     * Process a change in the bonding state for a device.
     *
     * @param device the device whose bonding state has changed
     * @param bondState the new bond state for the device. Possible values are:
     * {@link BluetoothDevice#BOND_NONE},
     * {@link BluetoothDevice#BOND_BONDING},
     * {@link BluetoothDevice#BOND_BONDED}.
     */
    @VisibleForTesting
    void bondStateChanged(BluetoothDevice device, int bondState) {
        if (DBG) {
            Log.d(TAG, "Bond state changed for device: " + device + " state: " + bondState);
        }
        // Remove state machine if the bonding for a device is removed
        if (bondState != BluetoothDevice.BOND_NONE) {
            return;
        }

        synchronized (mStateMachines) {
             AtpLocatorStateMachine sm = mStateMachines.get(device);
             if (sm == null) {
                 return;
             }
             if (sm.getConnectionState() != BluetoothProfile.STATE_DISCONNECTED) {
                 return;
             }
             mConnectionMode.remove(device);
             removeStateMachine(device);
        }
    }

    private void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            AtpLocatorStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                Log.w(TAG, "removeStateMachine: device " + device
                        + " does not have a state machine");
                return;
            }
            Log.i(TAG, "removeStateMachine: removing state machine for device: " + device);
            sm.doQuit();
            sm.cleanup();
            mStateMachines.remove(device);
        }
    }

    private AtpLocatorStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mStateMachines) {
            AtpLocatorStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                return sm;
            }
            if (mStateMachines.size() >= MAX_ATP_STATE_MACHINES) {
                Log.e(TAG, "Maximum number of ATP state machines reached: "
                        + MAX_ATP_STATE_MACHINES);
                return null;
            }
            if (DBG) {
                Log.d(TAG, "Creating a new state machine for " + device);
            }
            sm = AtpLocatorStateMachine.make(device, this, mContext,
                    mNativeInterface, mStateMachinesThread.getLooper());
            mStateMachines.put(device, sm);
            return sm;
        }
    }
}
