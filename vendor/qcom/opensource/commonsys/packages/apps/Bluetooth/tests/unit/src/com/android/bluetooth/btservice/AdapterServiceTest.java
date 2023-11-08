/*
 * Copyright 2017 The Android Open Source Project
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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

import android.app.AlarmManager;
import android.app.admin.DevicePolicyManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.IBluetoothCallback;
import android.content.AttributionSource;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.media.AudioManager;
import android.os.Binder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.Process;
import android.os.SystemProperties;
import android.os.UserManager;
import android.test.mock.MockContentResolver;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.R;
import com.android.bluetooth.TestUtils;
import com.android.bluetooth.Utils;

import libcore.util.HexEncoding;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashMap;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class AdapterServiceTest {
    private static final String TAG = AdapterServiceTest.class.getSimpleName();

    private AdapterService mAdapterService;
    private AdapterService.AdapterServiceBinder mServiceBinder;

    private @Mock Context mMockContext;
    private @Mock ApplicationInfo mMockApplicationInfo;
    private @Mock AlarmManager mMockAlarmManager;
    private @Mock Resources mMockResources;
    private @Mock UserManager mMockUserManager;
    private @Mock DevicePolicyManager mMockDevicePolicyManager;
    private @Mock ProfileService mMockGattService;
    private @Mock ProfileService mMockService;
    private @Mock ProfileService mMockService2;
    private @Mock IBluetoothCallback mIBluetoothCallback;
    private @Mock Binder mBinder;
    private @Mock AudioManager mAudioManager;

    private static final int CONTEXT_SWITCH_MS = 100;
    private static final int ONE_SECOND_MS = 1000;
    private static final int NATIVE_INIT_MS = 8000;
    private static final int NATIVE_DISABLE_MS = 1000;

    private final AttributionSource mAttributionSource = new AttributionSource.Builder(
            Process.myUid()).build();

    private PowerManager mPowerManager;
    private PackageManager mMockPackageManager;
    private MockContentResolver mMockContentResolver;
    private HashMap<String, HashMap<String, String>> mAdapterConfig;

    @BeforeClass
    public static void setupClass() {
        // Bring native layer up and down to make sure config files are properly loaded
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        Assert.assertNotNull(Looper.myLooper());
        AdapterService adapterService = new AdapterService();
        adapterService.initNative(false /* is_restricted */, false /* is_common_criteria_mod */,
                 0 /* config_compare_result */, false,
                 new String[0]);
        adapterService.cleanupNative();
        HashMap<String, HashMap<String, String>> adapterConfig = TestUtils.readAdapterConfig();
        Assert.assertNotNull(adapterConfig);
        Assert.assertNotNull("metrics salt is null: " + adapterConfig.toString(),
                getMetricsSalt(adapterConfig));
    }

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        Assert.assertNotNull(Looper.myLooper());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mAdapterService = new AdapterService());
        mServiceBinder = new AdapterService.AdapterServiceBinder(mAdapterService);
        mMockPackageManager = mock(PackageManager.class);
        mMockContentResolver = new MockContentResolver(mMockContext);
        MockitoAnnotations.initMocks(this);
        mPowerManager = (PowerManager) InstrumentationRegistry.getTargetContext()
                .getSystemService(Context.POWER_SERVICE);

        when(mMockContext.getApplicationInfo()).thenReturn(mMockApplicationInfo);
        when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
        when(mMockContext.getApplicationContext()).thenReturn(mMockContext);
        when(mMockContext.getResources()).thenReturn(mMockResources);
        when(mMockContext.getUserId()).thenReturn(Process.BLUETOOTH_UID);
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockContext.getSystemService(Context.USER_SERVICE)).thenReturn(mMockUserManager);
        when(mMockContext.getSystemService(Context.DEVICE_POLICY_SERVICE)).thenReturn(
                mMockDevicePolicyManager);
        when(mMockContext.getSystemService(Context.POWER_SERVICE)).thenReturn(mPowerManager);
        when(mMockContext.getSystemService(Context.ALARM_SERVICE)).thenReturn(mMockAlarmManager);
        when(mMockContext.getSystemService(Context.AUDIO_SERVICE)).thenReturn(mAudioManager);
        doAnswer(invocation -> {
            Object[] args = invocation.getArguments();
            return InstrumentationRegistry.getTargetContext().getDatabasePath((String) args[0]);
        }).when(mMockContext).getDatabasePath(anyString());

        when(mMockResources.getBoolean(R.bool.profile_supported_gatt)).thenReturn(true);
        when(mMockResources.getBoolean(R.bool.profile_supported_pbap)).thenReturn(true);
        when(mMockResources.getBoolean(R.bool.profile_supported_pan)).thenReturn(true);

        when(mMockDevicePolicyManager.isCommonCriteriaModeEnabled(any())).thenReturn(false);

        when(mIBluetoothCallback.asBinder()).thenReturn(mBinder);

        doReturn(Process.BLUETOOTH_UID).when(mMockPackageManager)
                .getPackageUidAsUser(any(), anyInt(), anyInt());

        when(mMockGattService.getName()).thenReturn("GattService");
        when(mMockService.getName()).thenReturn("Service1");
        when(mMockService2.getName()).thenReturn("Service2");

        // Attach a context to the service for permission checks.
        mAdapterService.attach(mMockContext, null, null, null, null, null);

        mAdapterService.onCreate();
        mServiceBinder.registerCallback(mIBluetoothCallback, mAttributionSource);

        Config.init(mMockContext);

        mAdapterConfig = TestUtils.readAdapterConfig();
        Assert.assertNotNull(mAdapterConfig);
    }

    @After
    public void tearDown() {
        mServiceBinder.unregisterCallback(mIBluetoothCallback, mAttributionSource);
        mAdapterService.cleanup();
        Config.init(InstrumentationRegistry.getTargetContext());
    }

    private void verifyStateChange(int prevState, int currState, int callNumber, int timeoutMs) {
        try {
            verify(mIBluetoothCallback, timeout(timeoutMs)
                    .times(callNumber)).onBluetoothStateChange(prevState, currState);
        } catch (Exception e) {
            // the mocked onBluetoothStateChange doesn't throw exceptions
        }
    }

    private void doEnable(int invocationNumber, boolean onlyGatt) {
        Assert.assertFalse(mAdapterService.isEnabled());

        final int startServiceCalls = 2 * (onlyGatt ? 1 : 3); // Start and stop GATT + 2

        mAdapterService.enable();

        verifyStateChange(BluetoothAdapter.STATE_OFF, BluetoothAdapter.STATE_BLE_TURNING_ON,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        // Start GATT
        verify(mMockContext, timeout(CONTEXT_SWITCH_MS).times(
                startServiceCalls * invocationNumber + 1)).startService(any());
        mAdapterService.addProfile(mMockGattService);
        mAdapterService.onProfileServiceStateChanged(mMockGattService, BluetoothAdapter.STATE_ON);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_ON, BluetoothAdapter.STATE_BLE_ON,
                invocationNumber + 1, NATIVE_INIT_MS);

        mAdapterService.onLeServiceUp();

        verifyStateChange(BluetoothAdapter.STATE_BLE_ON, BluetoothAdapter.STATE_TURNING_ON,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        if (!onlyGatt) {
            // Start Mock PBAP and PAN services
            verify(mMockContext, timeout(ONE_SECOND_MS).times(
                    startServiceCalls * invocationNumber + 3)).startService(any());
            mAdapterService.addProfile(mMockService);
            mAdapterService.addProfile(mMockService2);
            mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_ON);
            mAdapterService.onProfileServiceStateChanged(mMockService2, BluetoothAdapter.STATE_ON);
        }

        verifyStateChange(BluetoothAdapter.STATE_TURNING_ON, BluetoothAdapter.STATE_ON,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        verify(mMockContext, timeout(CONTEXT_SWITCH_MS).times(2 * invocationNumber + 2))
                .sendBroadcast(any(), eq(android.Manifest.permission.BLUETOOTH));
        final int scanMode = mAdapterService.getScanMode();
        Assert.assertTrue(scanMode == BluetoothAdapter.SCAN_MODE_CONNECTABLE
                || scanMode == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        Assert.assertTrue(mAdapterService.isEnabled());
    }

    private void doDisable(int invocationNumber, boolean onlyGatt) {
        Assert.assertTrue(mAdapterService.isEnabled());

        final int startServiceCalls = 2 * (onlyGatt ? 1 : 3); // Start and stop GATT + 2

        mAdapterService.disable();

        verifyStateChange(BluetoothAdapter.STATE_ON, BluetoothAdapter.STATE_TURNING_OFF,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        if (!onlyGatt) {
            // Stop PBAP and PAN
            verify(mMockContext, timeout(ONE_SECOND_MS).times(
                    startServiceCalls * invocationNumber + 5)).startService(any());
            mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_OFF);
            mAdapterService.onProfileServiceStateChanged(mMockService2, BluetoothAdapter.STATE_OFF);
        }

        verifyStateChange(BluetoothAdapter.STATE_TURNING_OFF, BluetoothAdapter.STATE_BLE_ON,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        mAdapterService.onBrEdrDown();

        verifyStateChange(BluetoothAdapter.STATE_BLE_ON, BluetoothAdapter.STATE_BLE_TURNING_OFF,
                invocationNumber + 1, CONTEXT_SWITCH_MS);

        // Stop GATT
        verify(mMockContext, timeout(ONE_SECOND_MS).times(
                startServiceCalls * invocationNumber + startServiceCalls)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockGattService, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_OFF, BluetoothAdapter.STATE_OFF,
                invocationNumber + 1, NATIVE_DISABLE_MS);

        Assert.assertFalse(mAdapterService.isEnabled());
    }

    /**
     * Test: Turn Bluetooth on.
     * Check whether the AdapterService gets started.
     */
    @Test
    public void testEnable() {
        doEnable(0, false);
    }

    /**
     * Test: Turn Bluetooth on/off.
     * Check whether the AdapterService gets started and stopped.
     */
    @Test
    public void testEnableDisable() {
        doEnable(0, false);
        doDisable(0, false);
    }

    /**
     * Test: Turn Bluetooth on/off with only GATT supported.
     * Check whether the AdapterService gets started and stopped.
     */
    @Test
    public void testEnableDisableOnlyGatt() {
        Context mockContext = mock(Context.class);
        Resources mockResources = mock(Resources.class);

        when(mockContext.getApplicationInfo()).thenReturn(mMockApplicationInfo);
        when(mockContext.getContentResolver()).thenReturn(mMockContentResolver);
        when(mockContext.getApplicationContext()).thenReturn(mockContext);
        when(mockContext.getResources()).thenReturn(mockResources);
        when(mockContext.getUserId()).thenReturn(Process.BLUETOOTH_UID);
        when(mockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mockContext.getSystemService(Context.USER_SERVICE)).thenReturn(mMockUserManager);
        when(mockContext.getSystemService(Context.POWER_SERVICE)).thenReturn(mPowerManager);
        when(mockContext.getSystemService(Context.ALARM_SERVICE)).thenReturn(mMockAlarmManager);

        when(mockResources.getBoolean(R.bool.profile_supported_gatt)).thenReturn(true);

        Config.init(mockContext);
        doEnable(0, true);
        doDisable(0, true);
    }

    /**
     * Test: Don't start GATT
     * Check whether the AdapterService quits gracefully
     */
    @Test
    public void testGattStartTimeout() {
        Assert.assertFalse(mAdapterService.isEnabled());

        mAdapterService.enable();

        verifyStateChange(BluetoothAdapter.STATE_OFF, BluetoothAdapter.STATE_BLE_TURNING_ON, 1,
                CONTEXT_SWITCH_MS);

        // Start GATT
        verify(mMockContext, timeout(CONTEXT_SWITCH_MS).times(1)).startService(any());
        mAdapterService.addProfile(mMockGattService);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_ON,
                BluetoothAdapter.STATE_BLE_TURNING_OFF, 1,
                AdapterState.BLE_START_TIMEOUT_DELAY + CONTEXT_SWITCH_MS);

        // Stop GATT
        verify(mMockContext, timeout(AdapterState.BLE_STOP_TIMEOUT_DELAY + CONTEXT_SWITCH_MS)
                .times(2)).startService(any());

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_OFF, BluetoothAdapter.STATE_OFF, 1,
                NATIVE_DISABLE_MS);

        Assert.assertFalse(mAdapterService.isEnabled());
    }

    /**
     * Test: Don't stop GATT
     * Check whether the AdapterService quits gracefully
     */
    @Test
    public void testGattStopTimeout() {
        doEnable(0, false);
        Assert.assertTrue(mAdapterService.isEnabled());

        mAdapterService.disable();

        verifyStateChange(BluetoothAdapter.STATE_ON, BluetoothAdapter.STATE_TURNING_OFF, 1,
                CONTEXT_SWITCH_MS);

        // Stop PBAP and PAN
        verify(mMockContext, timeout(ONE_SECOND_MS).times(5)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_OFF);
        mAdapterService.onProfileServiceStateChanged(mMockService2, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_TURNING_OFF, BluetoothAdapter.STATE_BLE_ON, 1,
                CONTEXT_SWITCH_MS);

        mAdapterService.onBrEdrDown();

        verifyStateChange(BluetoothAdapter.STATE_BLE_ON, BluetoothAdapter.STATE_BLE_TURNING_OFF, 1,
                CONTEXT_SWITCH_MS);

        // Stop GATT
        verify(mMockContext, timeout(ONE_SECOND_MS).times(6)).startService(any());

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_OFF, BluetoothAdapter.STATE_OFF, 1,
                AdapterState.BLE_STOP_TIMEOUT_DELAY + NATIVE_DISABLE_MS);

        Assert.assertFalse(mAdapterService.isEnabled());
    }

    /**
     * Test: Don't start a classic profile
     * Check whether the AdapterService quits gracefully
     */
    @Test
    public void testProfileStartTimeout() {
        Assert.assertFalse(mAdapterService.isEnabled());

        mAdapterService.enable();

        verifyStateChange(BluetoothAdapter.STATE_OFF, BluetoothAdapter.STATE_BLE_TURNING_ON, 1,
                CONTEXT_SWITCH_MS);

        // Start GATT
        verify(mMockContext, timeout(CONTEXT_SWITCH_MS).times(1)).startService(any());
        mAdapterService.addProfile(mMockGattService);
        mAdapterService.onProfileServiceStateChanged(mMockGattService, BluetoothAdapter.STATE_ON);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_ON, BluetoothAdapter.STATE_BLE_ON, 1,
                NATIVE_INIT_MS);

        mAdapterService.onLeServiceUp();

        verifyStateChange(BluetoothAdapter.STATE_BLE_ON, BluetoothAdapter.STATE_TURNING_ON, 1,
                CONTEXT_SWITCH_MS);

        // Register Mock PBAP and PAN services, only start one
        verify(mMockContext, timeout(ONE_SECOND_MS).times(3)).startService(any());
        mAdapterService.addProfile(mMockService);
        mAdapterService.addProfile(mMockService2);
        mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_ON);

        verifyStateChange(BluetoothAdapter.STATE_TURNING_ON, BluetoothAdapter.STATE_TURNING_OFF, 1,
                AdapterState.BREDR_START_TIMEOUT_DELAY + CONTEXT_SWITCH_MS);

        // Stop PBAP and PAN
        verify(mMockContext, timeout(ONE_SECOND_MS).times(5)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_TURNING_OFF, BluetoothAdapter.STATE_BLE_ON, 1,
                CONTEXT_SWITCH_MS);
    }

    /**
     * Test: Don't stop a classic profile
     * Check whether the AdapterService quits gracefully
     */
    @Test
    public void testProfileStopTimeout() {
        doEnable(0, false);

        Assert.assertTrue(mAdapterService.isEnabled());

        mAdapterService.disable();

        verifyStateChange(BluetoothAdapter.STATE_ON, BluetoothAdapter.STATE_TURNING_OFF, 1,
                CONTEXT_SWITCH_MS);

        // Stop PBAP and PAN
        verify(mMockContext, timeout(ONE_SECOND_MS).times(5)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_TURNING_OFF,
                BluetoothAdapter.STATE_BLE_TURNING_OFF, 1,
                AdapterState.BREDR_STOP_TIMEOUT_DELAY + CONTEXT_SWITCH_MS);

        // Stop GATT
        verify(mMockContext, timeout(ONE_SECOND_MS).times(6)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockGattService, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_OFF, BluetoothAdapter.STATE_OFF, 1,
                AdapterState.BLE_STOP_TIMEOUT_DELAY + NATIVE_DISABLE_MS);

        Assert.assertFalse(mAdapterService.isEnabled());
    }

    /**
     * Test: Toggle snoop logging setting
     * Check whether the AdapterService restarts fully
     */
    @Test
    public void testSnoopLoggingChange() {
        String snoopSetting =
                SystemProperties.get(AdapterService.BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "");
        SystemProperties.set(AdapterService.BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "false");
        doEnable(0, false);

        Assert.assertTrue(mAdapterService.isEnabled());

        Assert.assertFalse(
                SystemProperties.get(AdapterService.BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY,
                        "true").equals("true"));

        SystemProperties.set(AdapterService.BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "true");

        mAdapterService.disable();

        verifyStateChange(BluetoothAdapter.STATE_ON, BluetoothAdapter.STATE_TURNING_OFF, 1,
                CONTEXT_SWITCH_MS);

        // Stop PBAP and PAN
        verify(mMockContext, timeout(ONE_SECOND_MS).times(5)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockService, BluetoothAdapter.STATE_OFF);
        mAdapterService.onProfileServiceStateChanged(mMockService2, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_TURNING_OFF, BluetoothAdapter.STATE_BLE_ON, 1,
                CONTEXT_SWITCH_MS);

        // Don't call onBrEdrDown().  The Adapter should turn itself off.

        verifyStateChange(BluetoothAdapter.STATE_BLE_ON, BluetoothAdapter.STATE_BLE_TURNING_OFF, 1,
                CONTEXT_SWITCH_MS);

        // Stop GATT
        verify(mMockContext, timeout(ONE_SECOND_MS).times(6)).startService(any());
        mAdapterService.onProfileServiceStateChanged(mMockGattService, BluetoothAdapter.STATE_OFF);

        verifyStateChange(BluetoothAdapter.STATE_BLE_TURNING_OFF, BluetoothAdapter.STATE_OFF, 1,
                NATIVE_DISABLE_MS);

        Assert.assertFalse(mAdapterService.isEnabled());

        // Restore earlier setting
        SystemProperties.set(AdapterService.BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, snoopSetting);
    }


    /**
     * Test: Obfuscate a null Bluetooth
     * Check if returned value from {@link AdapterService#obfuscateAddress(BluetoothDevice)} is
     * an empty array when device address is null
     */
    @Test
    public void testObfuscateBluetoothAddress_NullAddress() {
        Assert.assertArrayEquals(mAdapterService.obfuscateAddress(null), new byte[0]);
    }

    /**
     * Test: Obfuscate Bluetooth address when Bluetooth is disabled
     * Check whether the returned value meets expectation
     */
    @Test
    public void testObfuscateBluetoothAddress_BluetoothDisabled() {
        Assert.assertFalse(mAdapterService.isEnabled());
        byte[] metricsSalt = getMetricsSalt(mAdapterConfig);
        Assert.assertNotNull(metricsSalt);
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress));
        Assert.assertArrayEquals(obfuscateInJava(metricsSalt, device), obfuscatedAddress);
    }

    /**
     * Test: Obfuscate Bluetooth address when Bluetooth is enabled
     * Check whether the returned value meets expectation
     */
    @Test
    public void testObfuscateBluetoothAddress_BluetoothEnabled() {
        Assert.assertFalse(mAdapterService.isEnabled());
        doEnable(0, false);
        Assert.assertTrue(mAdapterService.isEnabled());
        byte[] metricsSalt = getMetricsSalt(mAdapterConfig);
        Assert.assertNotNull(metricsSalt);
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress));
        Assert.assertArrayEquals(obfuscateInJava(metricsSalt, device), obfuscatedAddress);
    }

    /**
     * Test: Check if obfuscated Bluetooth address stays the same after toggling Bluetooth
     */
    @Test
    public void testObfuscateBluetoothAddress_PersistentBetweenToggle() {
        Assert.assertFalse(mAdapterService.isEnabled());
        byte[] metricsSalt = getMetricsSalt(mAdapterConfig);
        Assert.assertNotNull(metricsSalt);
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress1 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress1.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress1));
        Assert.assertArrayEquals(obfuscateInJava(metricsSalt, device),
                obfuscatedAddress1);
        // Enable
        doEnable(0, false);
        Assert.assertTrue(mAdapterService.isEnabled());
        byte[] obfuscatedAddress3 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress3.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress3));
        Assert.assertArrayEquals(obfuscatedAddress3,
                obfuscatedAddress1);
        // Disable
        doDisable(0, false);
        Assert.assertFalse(mAdapterService.isEnabled());
        byte[] obfuscatedAddress4 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress4.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress4));
        Assert.assertArrayEquals(obfuscatedAddress4,
                obfuscatedAddress1);
    }

    /**
     * Test: Check if obfuscated Bluetooth address stays the same after re-initializing
     *       {@link AdapterService}
     */
    @Test
    public void testObfuscateBluetoothAddress_PersistentBetweenAdapterServiceInitialization() throws
            PackageManager.NameNotFoundException {
        byte[] metricsSalt = getMetricsSalt(mAdapterConfig);
        Assert.assertNotNull(metricsSalt);
        Assert.assertFalse(mAdapterService.isEnabled());
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress1 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress1.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress1));
        Assert.assertArrayEquals(obfuscateInJava(metricsSalt, device),
                obfuscatedAddress1);
        tearDown();
        setUp();
        Assert.assertFalse(mAdapterService.isEnabled());
        byte[] obfuscatedAddress2 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress2.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress2));
        Assert.assertArrayEquals(obfuscatedAddress2,
                obfuscatedAddress1);
    }

    /**
     * Test: Verify that obfuscated Bluetooth address changes after factory reset
     *
     * There are 4 types of factory reset that we are talking about:
     * 1. Factory reset all user data from Settings -> Will restart phone
     * 2. Factory reset WiFi and Bluetooth from Settings -> Will only restart WiFi and BT
     * 3. Call BluetoothAdapter.factoryReset() -> Will disable Bluetooth and reset config in
     * memory and disk
     * 4. Call AdapterService.factoryReset() -> Will only reset config in memory
     *
     * We can only use No. 4 here
     */
    @Ignore("AdapterService.factoryReset() does not reload config into memory and hence old salt"
            + " is still used until next time Bluetooth library is initialized. However Bluetooth"
            + " cannot be used until Bluetooth process restart any way. Thus it is almost"
            + " guaranteed that user has to re-enable Bluetooth and hence re-generate new salt"
            + " after factory reset")
    @Test
    public void testObfuscateBluetoothAddress_FactoryReset() {
        Assert.assertFalse(mAdapterService.isEnabled());
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress1 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress1.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress1));
        mServiceBinder.factoryReset(mAttributionSource);
        byte[] obfuscatedAddress2 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress2.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress2));
        Assert.assertFalse(Arrays.equals(obfuscatedAddress2,
                obfuscatedAddress1));
        doEnable(0, false);
        byte[] obfuscatedAddress3 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress3.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress3));
        Assert.assertArrayEquals(obfuscatedAddress3,
                obfuscatedAddress2);
        mServiceBinder.factoryReset(mAttributionSource);
        byte[] obfuscatedAddress4 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress4.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress4));
        Assert.assertFalse(Arrays.equals(obfuscatedAddress4,
                obfuscatedAddress3));
    }

    /**
     * Test: Verify that obfuscated Bluetooth address changes after factory reset and reloading
     *       native layer
     */
    @Test
    public void testObfuscateBluetoothAddress_FactoryResetAndReloadNativeLayer() throws
            PackageManager.NameNotFoundException {
        byte[] metricsSalt1 = getMetricsSalt(mAdapterConfig);
        Assert.assertNotNull(metricsSalt1);
        Assert.assertFalse(mAdapterService.isEnabled());
        BluetoothDevice device = TestUtils.getTestDevice(BluetoothAdapter.getDefaultAdapter(), 0);
        byte[] obfuscatedAddress1 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress1.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress1));
        Assert.assertArrayEquals(obfuscateInJava(metricsSalt1, device),
                obfuscatedAddress1);
        mServiceBinder.factoryReset(mAttributionSource);
        tearDown();
        setUp();
        // Cannot verify metrics salt since it is not written to disk until native cleanup
        byte[] obfuscatedAddress2 = mAdapterService.obfuscateAddress(device);
        Assert.assertTrue(obfuscatedAddress2.length > 0);
        Assert.assertFalse(isByteArrayAllZero(obfuscatedAddress2));
        Assert.assertFalse(Arrays.equals(obfuscatedAddress2,
                obfuscatedAddress1));
    }

    private static byte[] getMetricsSalt(HashMap<String, HashMap<String, String>> adapterConfig) {
        HashMap<String, String> metricsSection = adapterConfig.get("Metrics");
        if (metricsSection == null) {
            Log.e(TAG, "Metrics section is null: " + adapterConfig.toString());
            return null;
        }
        String saltString = metricsSection.get("Salt256Bit");
        if (saltString == null) {
            Log.e(TAG, "Salt256Bit is null: " + metricsSection.toString());
            return null;
        }
        byte[] metricsSalt = HexEncoding.decode(saltString, false /* allowSingleChar */);
        if (metricsSalt.length != 32) {
            Log.e(TAG, "Salt length is not 32 bit, but is " + metricsSalt.length);
            return null;
        }
        return metricsSalt;
    }

    private static byte[] obfuscateInJava(byte[] key, BluetoothDevice device) {
        String algorithm = "HmacSHA256";
        try {
            Mac hmac256 = Mac.getInstance(algorithm);
            hmac256.init(new SecretKeySpec(key, algorithm));
            return hmac256.doFinal(Utils.getByteAddress(device));
        } catch (NoSuchAlgorithmException | IllegalStateException | InvalidKeyException exp) {
            exp.printStackTrace();
            return null;
        }
    }

    private static boolean isByteArrayAllZero(byte[] byteArray) {
        for (byte i : byteArray) {
            if (i != 0) {
                return false;
            }
        }
        return true;
    }
}
