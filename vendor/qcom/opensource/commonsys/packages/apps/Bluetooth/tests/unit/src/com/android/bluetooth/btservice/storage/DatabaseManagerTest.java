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

package com.android.bluetooth.btservice.storage;

import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;

import androidx.room.Room;
import androidx.room.testing.MigrationTestHelper;
import androidx.sqlite.db.SupportSQLiteDatabase;
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.bluetooth.TestUtils;
import com.android.bluetooth.btservice.AdapterService;

import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.IOException;
import java.util.List;

@MediumTest
@RunWith(AndroidJUnit4.class)
public final class DatabaseManagerTest {

    @Mock private AdapterService mAdapterService;

    private MetadataDatabase mDatabase;
    private DatabaseManager mDatabaseManager;
    private BluetoothDevice mTestDevice;
    private BluetoothDevice mTestDevice2;
    private BluetoothDevice mTestDevice3;

    private static final String LOCAL_STORAGE = "LocalStorage";
    private static final String TEST_BT_ADDR = "11:22:33:44:55:66";
    private static final String TEST_BT_ADDR2 = "66:55:44:33:22:11";
    private static final String TEST_BT_ADDR3 = "12:34:56:65:43:21";
    private static final String OTHER_BT_ADDR1 = "11:11:11:11:11:11";
    private static final String OTHER_BT_ADDR2 = "22:22:22:22:22:22";
    private static final String DB_NAME = "test_db";
    private static final int A2DP_SUPPORT_OP_CODEC_TEST = 0;
    private static final int A2DP_ENALBED_OP_CODEC_TEST = 1;
    private static final int MAX_META_ID = 16;
    private static final byte[] TEST_BYTE_ARRAY = "TEST_VALUE".getBytes();

    @Rule
    public MigrationTestHelper testHelper = new MigrationTestHelper(
            InstrumentationRegistry.getInstrumentation(),
            MetadataDatabase.class.getCanonicalName(),
            new FrameworkSQLiteOpenHelperFactory());
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        TestUtils.setAdapterService(mAdapterService);

        mTestDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(TEST_BT_ADDR);
        mTestDevice2 = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(TEST_BT_ADDR2);
        mTestDevice3 = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(TEST_BT_ADDR3);

        // Create a memory database for DatabaseManager instead of use a real database.
        mDatabase = Room.inMemoryDatabaseBuilder(InstrumentationRegistry.getTargetContext(),
                MetadataDatabase.class).build();

        when(mAdapterService.getPackageManager()).thenReturn(
                InstrumentationRegistry.getTargetContext().getPackageManager());
        mDatabaseManager = new DatabaseManager(mAdapterService);

        BluetoothDevice[] bondedDevices = {mTestDevice};
        doReturn(bondedDevices).when(mAdapterService).getBondedDevices();
        doNothing().when(mAdapterService).metadataChanged(
                anyString(), anyInt(), any(byte[].class));

        restartDatabaseManagerHelper();
    }

    @After
    public void tearDown() throws Exception {
        TestUtils.clearAdapterService(mAdapterService);
        mDatabase.deleteAll();
        mDatabaseManager.cleanup();
    }

    @Test
    public void testMetadataDefault() {
        Metadata data = new Metadata(TEST_BT_ADDR);
        mDatabase.insert(data);
        restartDatabaseManagerHelper();

        for (int id = 0; id < BluetoothProfile.MAX_PROFILE_ID; id++) {
            Assert.assertEquals(BluetoothProfile.CONNECTION_POLICY_UNKNOWN,
                    mDatabaseManager.getProfileConnectionPolicy(mTestDevice, id));
        }

        Assert.assertEquals(BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN,
                mDatabaseManager.getA2dpSupportsOptionalCodecs(mTestDevice));

        Assert.assertEquals(BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN,
                    mDatabaseManager.getA2dpOptionalCodecsEnabled(mTestDevice));

        for (int id = 0; id < MAX_META_ID; id++) {
            Assert.assertNull(mDatabaseManager.getCustomMeta(mTestDevice, id));
        }

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    @Test
    public void testSetGetProfileConnectionPolicy() {
        int badConnectionPolicy = -100;

        // Cases of device not in database
        testSetGetProfileConnectionPolicyCase(false, BluetoothProfile.CONNECTION_POLICY_UNKNOWN,
                BluetoothProfile.CONNECTION_POLICY_UNKNOWN, true);
        testSetGetProfileConnectionPolicyCase(false, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN,
                BluetoothProfile.CONNECTION_POLICY_FORBIDDEN, true);
        testSetGetProfileConnectionPolicyCase(false, BluetoothProfile.CONNECTION_POLICY_ALLOWED,
                BluetoothProfile.CONNECTION_POLICY_ALLOWED, true);
        testSetGetProfileConnectionPolicyCase(false, badConnectionPolicy,
                BluetoothProfile.CONNECTION_POLICY_UNKNOWN, false);

        // Cases of device already in database
        testSetGetProfileConnectionPolicyCase(true, BluetoothProfile.CONNECTION_POLICY_UNKNOWN,
                BluetoothProfile.CONNECTION_POLICY_UNKNOWN, true);
        testSetGetProfileConnectionPolicyCase(true, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN,
                BluetoothProfile.CONNECTION_POLICY_FORBIDDEN, true);
        testSetGetProfileConnectionPolicyCase(true, BluetoothProfile.CONNECTION_POLICY_ALLOWED,
                BluetoothProfile.CONNECTION_POLICY_ALLOWED, true);
        testSetGetProfileConnectionPolicyCase(true, badConnectionPolicy,
                BluetoothProfile.CONNECTION_POLICY_UNKNOWN, false);
    }

    @Test
    public void testSetGetA2dpSupportsOptionalCodecs() {
        int badValue = -100;

        // Cases of device not in database
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORTED,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, false,
                badValue, BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);

        // Cases of device already in database
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED,
                BluetoothA2dp.OPTIONAL_CODECS_NOT_SUPPORTED);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORTED,
                BluetoothA2dp.OPTIONAL_CODECS_SUPPORTED);
        testSetGetA2dpOptionalCodecsCase(A2DP_SUPPORT_OP_CODEC_TEST, true,
                badValue, BluetoothA2dp.OPTIONAL_CODECS_SUPPORT_UNKNOWN);
    }

    @Test
    public void testSetGetA2dpOptionalCodecsEnabled() {
        int badValue = -100;

        // Cases of device not in database
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_DISABLED,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, false,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_ENABLED,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, false,
                badValue, BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);

        // Cases of device already in database
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_DISABLED,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_DISABLED);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, true,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_ENABLED,
                BluetoothA2dp.OPTIONAL_CODECS_PREF_ENABLED);
        testSetGetA2dpOptionalCodecsCase(A2DP_ENALBED_OP_CODEC_TEST, true,
                badValue, BluetoothA2dp.OPTIONAL_CODECS_PREF_UNKNOWN);
    }

    @Test
    public void testRemoveUnusedMetadata_WithSingleBondedDevice() {
        // Insert two devices to database and cache, only mTestDevice is
        // in the bonded list
        Metadata otherData = new Metadata(OTHER_BT_ADDR1);
        // Add metadata for otherDevice
        otherData.setCustomizedMeta(0, TEST_BYTE_ARRAY);
        mDatabaseManager.mMetadataCache.put(OTHER_BT_ADDR1, otherData);
        mDatabase.insert(otherData);

        Metadata data = new Metadata(TEST_BT_ADDR);
        mDatabaseManager.mMetadataCache.put(TEST_BT_ADDR, data);
        mDatabase.insert(data);

        mDatabaseManager.removeUnusedMetadata();
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        // Check removed device report metadata changed to null
        verify(mAdapterService).metadataChanged(OTHER_BT_ADDR1, 0, null);

        List<Metadata> list = mDatabase.load();

        // Check number of metadata in the database
        Assert.assertEquals(1, list.size());

        // Check whether the device is in database
        Metadata checkData = list.get(0);
        Assert.assertEquals(TEST_BT_ADDR, checkData.getAddress());

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    @Test
    public void testRemoveUnusedMetadata_WithMultiBondedDevices() {
        // Insert three devices to database and cache, otherDevice1 and otherDevice2
        // are in the bonded list

        // Add metadata for TEST_BT_ADDR
        Metadata testData = new Metadata(TEST_BT_ADDR);
        testData.setCustomizedMeta(0, TEST_BYTE_ARRAY);
        mDatabaseManager.mMetadataCache.put(TEST_BT_ADDR, testData);
        mDatabase.insert(testData);

        // Add metadata for OTHER_BT_ADDR1
        Metadata otherData1 = new Metadata(OTHER_BT_ADDR1);
        otherData1.setCustomizedMeta(0, TEST_BYTE_ARRAY);
        mDatabaseManager.mMetadataCache.put(OTHER_BT_ADDR1, otherData1);
        mDatabase.insert(otherData1);

        // Add metadata for OTHER_BT_ADDR2
        Metadata otherData2 = new Metadata(OTHER_BT_ADDR2);
        otherData2.setCustomizedMeta(0, TEST_BYTE_ARRAY);
        mDatabaseManager.mMetadataCache.put(OTHER_BT_ADDR2, otherData2);
        mDatabase.insert(otherData2);

        // Add OTHER_BT_ADDR1 OTHER_BT_ADDR2 to bonded devices
        BluetoothDevice otherDevice1 = BluetoothAdapter.getDefaultAdapter()
                .getRemoteDevice(OTHER_BT_ADDR1);
        BluetoothDevice otherDevice2 = BluetoothAdapter.getDefaultAdapter()
                .getRemoteDevice(OTHER_BT_ADDR2);
        BluetoothDevice[] bondedDevices = {otherDevice1, otherDevice2};
        doReturn(bondedDevices).when(mAdapterService).getBondedDevices();

        mDatabaseManager.removeUnusedMetadata();
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        // Check TEST_BT_ADDR report metadata changed to null
        verify(mAdapterService).metadataChanged(TEST_BT_ADDR, 0, null);

        // Check number of metadata in the database
        List<Metadata> list = mDatabase.load();
        // OTHER_BT_ADDR1 and OTHER_BT_ADDR2 should still in database
        Assert.assertEquals(2, list.size());

        // Check whether the devices are in the database
        Metadata checkData1 = list.get(0);
        Assert.assertEquals(OTHER_BT_ADDR2, checkData1.getAddress());
        Metadata checkData2 = list.get(1);
        Assert.assertEquals(OTHER_BT_ADDR1, checkData2.getAddress());

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

    }

    @Test
    public void testSetGetCustomMeta() {
        int badKey = 100;
        byte[] value = "input value".getBytes();

        // Device is not in database
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_MANUFACTURER_NAME,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_MODEL_NAME,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_SOFTWARE_VERSION,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_HARDWARE_VERSION,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_COMPANION_APP,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_MAIN_ICON,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_IS_UNTETHERED_HEADSET,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_LEFT_ICON,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_RIGHT_ICON,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_CASE_ICON,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_LEFT_BATTERY,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_RIGHT_BATTERY,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_CASE_BATTERY,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_LEFT_CHARGING,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_RIGHT_CHARGING,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_UNTETHERED_CASE_CHARGING,
                value, true);
        testSetGetCustomMetaCase(false, BluetoothDevice.METADATA_ENHANCED_SETTINGS_UI_URI,
                value, true);
        testSetGetCustomMetaCase(false, badKey, value, false);

        // Device is in database
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_MANUFACTURER_NAME,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_MODEL_NAME,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_SOFTWARE_VERSION,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_HARDWARE_VERSION,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_COMPANION_APP,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_MAIN_ICON,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_IS_UNTETHERED_HEADSET,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_LEFT_ICON,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_RIGHT_ICON,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_CASE_ICON,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_LEFT_BATTERY,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_RIGHT_BATTERY,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_CASE_BATTERY,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_LEFT_CHARGING,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_RIGHT_CHARGING,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_UNTETHERED_CASE_CHARGING,
                value, true);
        testSetGetCustomMetaCase(true, BluetoothDevice.METADATA_ENHANCED_SETTINGS_UI_URI,
                value, true);
    }

    @Test
    public void testSetConnection() {
        // Verify pre-conditions to ensure a fresh test
        Assert.assertEquals(0, mDatabaseManager.mMetadataCache.size());
        Assert.assertNotNull(mTestDevice);
        Assert.assertNotNull(mTestDevice2);
        Assert.assertNull(mDatabaseManager.getMostRecentlyConnectedA2dpDevice());

        // Set the first device's connection
        mDatabaseManager.setConnection(mTestDevice, true);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        List<BluetoothDevice> mostRecentlyConnectedDevicesOrdered =
                mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(mTestDevice, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        Assert.assertEquals(1, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(0));

        // Setting the second device's connection
        mDatabaseManager.setConnection(mTestDevice2, true);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertEquals(mTestDevice2, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(2, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));

        // Connect first test device again
        mDatabaseManager.setConnection(mTestDevice, true);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertEquals(mTestDevice, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(2, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(1));

        // Disconnect first test device's connection
        mDatabaseManager.setDisconnection(mTestDevice);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertNull(mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(2, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(1));

        // Connect third test device (non-a2dp device)
        mDatabaseManager.setConnection(mTestDevice3, false);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertNull(mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        // Connect first test device again
        mDatabaseManager.setConnection(mTestDevice, true);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertEquals(mTestDevice, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        // Connect third test device again and ensure it doesn't reset active a2dp device
        mDatabaseManager.setConnection(mTestDevice3, false);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertEquals(mTestDevice, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        // Disconnect second test device
        mDatabaseManager.setDisconnection(mTestDevice2);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertTrue(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertEquals(mTestDevice, mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        // Disconnect first test device
        mDatabaseManager.setDisconnection(mTestDevice);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertNull(mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        // Disconnect third test device
        mDatabaseManager.setDisconnection(mTestDevice3);
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice2.getAddress()).is_active_a2dp_device);
        Assert.assertFalse(mDatabaseManager
                .mMetadataCache.get(mTestDevice3.getAddress()).is_active_a2dp_device);
        Assert.assertNull(mDatabaseManager.getMostRecentlyConnectedA2dpDevice());
        mostRecentlyConnectedDevicesOrdered = mDatabaseManager.getMostRecentlyConnectedDevices();
        Assert.assertEquals(3, mostRecentlyConnectedDevicesOrdered.size());
        Assert.assertEquals(mTestDevice3, mostRecentlyConnectedDevicesOrdered.get(0));
        Assert.assertEquals(mTestDevice, mostRecentlyConnectedDevicesOrdered.get(1));
        Assert.assertEquals(mTestDevice2, mostRecentlyConnectedDevicesOrdered.get(2));

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    @Test
    public void testDatabaseMigration_100_101() throws IOException {
        // Create a database with version 100
        SupportSQLiteDatabase db = testHelper.createDatabase(DB_NAME, 100);
        Cursor cursor = db.query("SELECT * FROM metadata");

        // pbap_client_priority should not in version 100
        assertHasColumn(cursor, "pbap_client_priority", false);

        // Migrate database from 100 to 101
        db.close();
        db = testHelper.runMigrationsAndValidate(DB_NAME, 101, true,
                MetadataDatabase.MIGRATION_100_101);
        cursor = db.query("SELECT * FROM metadata");

        // Check whether pbap_client_priority exists in version 101
        assertHasColumn(cursor, "pbap_client_priority", true);
    }

    @Test
    public void testDatabaseMigration_101_102() throws IOException {
        String testString = "TEST STRING";

        // Create a database with version 101
        SupportSQLiteDatabase db = testHelper.createDatabase(DB_NAME, 101);
        Cursor cursor = db.query("SELECT * FROM metadata");

        // insert a device to the database
        ContentValues device = new ContentValues();
        device.put("address", TEST_BT_ADDR);
        device.put("migrated", false);
        device.put("a2dpSupportsOptionalCodecs", -1);
        device.put("a2dpOptionalCodecsEnabled", -1);
        device.put("a2dp_priority", -1);
        device.put("a2dp_sink_priority", -1);
        device.put("hfp_priority", -1);
        device.put("hfp_client_priority", -1);
        device.put("hid_host_priority", -1);
        device.put("pan_priority", -1);
        device.put("pbap_priority", -1);
        device.put("pbap_client_priority", -1);
        device.put("map_priority", -1);
        device.put("sap_priority", -1);
        device.put("hearing_aid_priority", -1);
        device.put("map_client_priority", -1);
        device.put("manufacturer_name", testString);
        device.put("model_name", testString);
        device.put("software_version", testString);
        device.put("hardware_version", testString);
        device.put("companion_app", testString);
        device.put("main_icon", testString);
        device.put("is_unthethered_headset", testString);
        device.put("unthethered_left_icon", testString);
        device.put("unthethered_right_icon", testString);
        device.put("unthethered_case_icon", testString);
        device.put("unthethered_left_battery", testString);
        device.put("unthethered_right_battery", testString);
        device.put("unthethered_case_battery", testString);
        device.put("unthethered_left_charging", testString);
        device.put("unthethered_right_charging", testString);
        device.put("unthethered_case_charging", testString);
        assertThat(db.insert("metadata", SQLiteDatabase.CONFLICT_IGNORE, device),
                CoreMatchers.not(-1));

        // Check the metadata names on version 101
        assertHasColumn(cursor, "is_unthethered_headset", true);
        assertHasColumn(cursor, "unthethered_left_icon", true);
        assertHasColumn(cursor, "unthethered_right_icon", true);
        assertHasColumn(cursor, "unthethered_case_icon", true);
        assertHasColumn(cursor, "unthethered_left_battery", true);
        assertHasColumn(cursor, "unthethered_right_battery", true);
        assertHasColumn(cursor, "unthethered_case_battery", true);
        assertHasColumn(cursor, "unthethered_left_charging", true);
        assertHasColumn(cursor, "unthethered_right_charging", true);
        assertHasColumn(cursor, "unthethered_case_charging", true);

        // Migrate database from 101 to 102
        db.close();
        db = testHelper.runMigrationsAndValidate(DB_NAME, 102, true,
                MetadataDatabase.MIGRATION_101_102);
        cursor = db.query("SELECT * FROM metadata");

        // metadata names should be changed on version 102
        assertHasColumn(cursor, "is_unthethered_headset", false);
        assertHasColumn(cursor, "unthethered_left_icon", false);
        assertHasColumn(cursor, "unthethered_right_icon", false);
        assertHasColumn(cursor, "unthethered_case_icon", false);
        assertHasColumn(cursor, "unthethered_left_battery", false);
        assertHasColumn(cursor, "unthethered_right_battery", false);
        assertHasColumn(cursor, "unthethered_case_battery", false);
        assertHasColumn(cursor, "unthethered_left_charging", false);
        assertHasColumn(cursor, "unthethered_right_charging", false);
        assertHasColumn(cursor, "unthethered_case_charging", false);

        assertHasColumn(cursor, "is_untethered_headset", true);
        assertHasColumn(cursor, "untethered_left_icon", true);
        assertHasColumn(cursor, "untethered_right_icon", true);
        assertHasColumn(cursor, "untethered_case_icon", true);
        assertHasColumn(cursor, "untethered_left_battery", true);
        assertHasColumn(cursor, "untethered_right_battery", true);
        assertHasColumn(cursor, "untethered_case_battery", true);
        assertHasColumn(cursor, "untethered_left_charging", true);
        assertHasColumn(cursor, "untethered_right_charging", true);
        assertHasColumn(cursor, "untethered_case_charging", true);

        while (cursor.moveToNext()) {
            // Check whether metadata data type are blob
            assertColumnBlob(cursor, "manufacturer_name");
            assertColumnBlob(cursor, "model_name");
            assertColumnBlob(cursor, "software_version");
            assertColumnBlob(cursor, "hardware_version");
            assertColumnBlob(cursor, "companion_app");
            assertColumnBlob(cursor, "main_icon");
            assertColumnBlob(cursor, "is_untethered_headset");
            assertColumnBlob(cursor, "untethered_left_icon");
            assertColumnBlob(cursor, "untethered_right_icon");
            assertColumnBlob(cursor, "untethered_case_icon");
            assertColumnBlob(cursor, "untethered_left_battery");
            assertColumnBlob(cursor, "untethered_right_battery");
            assertColumnBlob(cursor, "untethered_case_battery");
            assertColumnBlob(cursor, "untethered_left_charging");
            assertColumnBlob(cursor, "untethered_right_charging");
            assertColumnBlob(cursor, "untethered_case_charging");

            // Check whether metadata values are migrated to version 102 successfully
            assertColumnBlobData(cursor, "manufacturer_name", testString.getBytes());
            assertColumnBlobData(cursor, "model_name", testString.getBytes());
            assertColumnBlobData(cursor, "software_version", testString.getBytes());
            assertColumnBlobData(cursor, "hardware_version", testString.getBytes());
            assertColumnBlobData(cursor, "companion_app", testString.getBytes());
            assertColumnBlobData(cursor, "main_icon", testString.getBytes());
            assertColumnBlobData(cursor, "is_untethered_headset", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_charging", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_charging", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_charging", testString.getBytes());
        }
    }

    @Test
    public void testDatabaseMigration_102_103() throws IOException {
        String testString = "TEST STRING";

        // Create a database with version 102
        SupportSQLiteDatabase db = testHelper.createDatabase(DB_NAME, 102);
        Cursor cursor = db.query("SELECT * FROM metadata");

        // insert a device to the database
        ContentValues device = new ContentValues();
        device.put("address", TEST_BT_ADDR);
        device.put("migrated", false);
        device.put("a2dpSupportsOptionalCodecs", -1);
        device.put("a2dpOptionalCodecsEnabled", -1);
        device.put("a2dp_priority", 1000);
        device.put("a2dp_sink_priority", 1000);
        device.put("hfp_priority", 1000);
        device.put("hfp_client_priority", 1000);
        device.put("hid_host_priority", 1000);
        device.put("pan_priority", 1000);
        device.put("pbap_priority", 1000);
        device.put("pbap_client_priority", 1000);
        device.put("map_priority", 1000);
        device.put("sap_priority", 1000);
        device.put("hearing_aid_priority", 1000);
        device.put("map_client_priority", 1000);
        device.put("manufacturer_name", testString);
        device.put("model_name", testString);
        device.put("software_version", testString);
        device.put("hardware_version", testString);
        device.put("companion_app", testString);
        device.put("main_icon", testString);
        device.put("is_untethered_headset", testString);
        device.put("untethered_left_icon", testString);
        device.put("untethered_right_icon", testString);
        device.put("untethered_case_icon", testString);
        device.put("untethered_left_battery", testString);
        device.put("untethered_right_battery", testString);
        device.put("untethered_case_battery", testString);
        device.put("untethered_left_charging", testString);
        device.put("untethered_right_charging", testString);
        device.put("untethered_case_charging", testString);
        assertThat(db.insert("metadata", SQLiteDatabase.CONFLICT_IGNORE, device),
                CoreMatchers.not(-1));

        // Check the metadata names on version 102
        assertHasColumn(cursor, "a2dp_priority", true);
        assertHasColumn(cursor, "a2dp_sink_priority", true);
        assertHasColumn(cursor, "hfp_priority", true);
        assertHasColumn(cursor, "hfp_client_priority", true);
        assertHasColumn(cursor, "hid_host_priority", true);
        assertHasColumn(cursor, "pan_priority", true);
        assertHasColumn(cursor, "pbap_priority", true);
        assertHasColumn(cursor, "pbap_client_priority", true);
        assertHasColumn(cursor, "map_priority", true);
        assertHasColumn(cursor, "sap_priority", true);
        assertHasColumn(cursor, "hearing_aid_priority", true);
        assertHasColumn(cursor, "map_client_priority", true);

        // Migrate database from 102 to 103
        db.close();
        db = testHelper.runMigrationsAndValidate(DB_NAME, 103, true,
                MetadataDatabase.MIGRATION_102_103);
        cursor = db.query("SELECT * FROM metadata");

        // metadata names should be changed on version 103
        assertHasColumn(cursor, "a2dp_priority", false);
        assertHasColumn(cursor, "a2dp_sink_priority", false);
        assertHasColumn(cursor, "hfp_priority", false);
        assertHasColumn(cursor, "hfp_client_priority", false);
        assertHasColumn(cursor, "hid_host_priority", false);
        assertHasColumn(cursor, "pan_priority", false);
        assertHasColumn(cursor, "pbap_priority", false);
        assertHasColumn(cursor, "pbap_client_priority", false);
        assertHasColumn(cursor, "map_priority", false);
        assertHasColumn(cursor, "sap_priority", false);
        assertHasColumn(cursor, "hearing_aid_priority", false);
        assertHasColumn(cursor, "map_client_priority", false);

        assertHasColumn(cursor, "a2dp_connection_policy", true);
        assertHasColumn(cursor, "a2dp_sink_connection_policy", true);
        assertHasColumn(cursor, "hfp_connection_policy", true);
        assertHasColumn(cursor, "hfp_client_connection_policy", true);
        assertHasColumn(cursor, "hid_host_connection_policy", true);
        assertHasColumn(cursor, "pan_connection_policy", true);
        assertHasColumn(cursor, "pbap_connection_policy", true);
        assertHasColumn(cursor, "pbap_client_connection_policy", true);
        assertHasColumn(cursor, "map_connection_policy", true);
        assertHasColumn(cursor, "sap_connection_policy", true);
        assertHasColumn(cursor, "hearing_aid_connection_policy", true);
        assertHasColumn(cursor, "map_client_connection_policy", true);

        while (cursor.moveToNext()) {
            // Check PRIORITY_AUTO_CONNECT (1000) was replaced with CONNECTION_POLICY_ALLOWED (100)
            assertColumnIntData(cursor, "a2dp_connection_policy", 100);
            assertColumnIntData(cursor, "a2dp_sink_connection_policy", 100);
            assertColumnIntData(cursor, "hfp_connection_policy", 100);
            assertColumnIntData(cursor, "hfp_client_connection_policy", 100);
            assertColumnIntData(cursor, "hid_host_connection_policy", 100);
            assertColumnIntData(cursor, "pan_connection_policy", 100);
            assertColumnIntData(cursor, "pbap_connection_policy", 100);
            assertColumnIntData(cursor, "pbap_client_connection_policy", 100);
            assertColumnIntData(cursor, "map_connection_policy", 100);
            assertColumnIntData(cursor, "sap_connection_policy", 100);
            assertColumnIntData(cursor, "hearing_aid_connection_policy", 100);
            assertColumnIntData(cursor, "map_client_connection_policy", 100);

            // Check whether metadata data type are blob
            assertColumnBlob(cursor, "manufacturer_name");
            assertColumnBlob(cursor, "model_name");
            assertColumnBlob(cursor, "software_version");
            assertColumnBlob(cursor, "hardware_version");
            assertColumnBlob(cursor, "companion_app");
            assertColumnBlob(cursor, "main_icon");
            assertColumnBlob(cursor, "is_untethered_headset");
            assertColumnBlob(cursor, "untethered_left_icon");
            assertColumnBlob(cursor, "untethered_right_icon");
            assertColumnBlob(cursor, "untethered_case_icon");
            assertColumnBlob(cursor, "untethered_left_battery");
            assertColumnBlob(cursor, "untethered_right_battery");
            assertColumnBlob(cursor, "untethered_case_battery");
            assertColumnBlob(cursor, "untethered_left_charging");
            assertColumnBlob(cursor, "untethered_right_charging");
            assertColumnBlob(cursor, "untethered_case_charging");

            // Check whether metadata values are migrated to version 103 successfully
            assertColumnBlobData(cursor, "manufacturer_name", testString.getBytes());
            assertColumnBlobData(cursor, "model_name", testString.getBytes());
            assertColumnBlobData(cursor, "software_version", testString.getBytes());
            assertColumnBlobData(cursor, "hardware_version", testString.getBytes());
            assertColumnBlobData(cursor, "companion_app", testString.getBytes());
            assertColumnBlobData(cursor, "main_icon", testString.getBytes());
            assertColumnBlobData(cursor, "is_untethered_headset", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_icon", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_battery", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_left_charging", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_right_charging", testString.getBytes());
            assertColumnBlobData(cursor, "untethered_case_charging", testString.getBytes());
        }
    }

    @Test
    public void testDatabaseMigration_103_104() throws IOException {
        String testString = "TEST STRING";

        // Create a database with version 103
        SupportSQLiteDatabase db = testHelper.createDatabase(DB_NAME, 103);

        // insert a device to the database
        ContentValues device = new ContentValues();
        device.put("address", TEST_BT_ADDR);
        device.put("migrated", false);
        device.put("a2dpSupportsOptionalCodecs", -1);
        device.put("a2dpOptionalCodecsEnabled", -1);
        device.put("a2dp_connection_policy", 100);
        device.put("a2dp_sink_connection_policy", 100);
        device.put("hfp_connection_policy", 100);
        device.put("hfp_client_connection_policy", 100);
        device.put("hid_host_connection_policy", 100);
        device.put("pan_connection_policy", 100);
        device.put("pbap_connection_policy", 100);
        device.put("pbap_client_connection_policy", 100);
        device.put("map_connection_policy", 100);
        device.put("sap_connection_policy", 100);
        device.put("hearing_aid_connection_policy", 100);
        device.put("map_client_connection_policy", 100);
        device.put("manufacturer_name", testString);
        device.put("model_name", testString);
        device.put("software_version", testString);
        device.put("hardware_version", testString);
        device.put("companion_app", testString);
        device.put("main_icon", testString);
        device.put("is_untethered_headset", testString);
        device.put("untethered_left_icon", testString);
        device.put("untethered_right_icon", testString);
        device.put("untethered_case_icon", testString);
        device.put("untethered_left_battery", testString);
        device.put("untethered_right_battery", testString);
        device.put("untethered_case_battery", testString);
        device.put("untethered_left_charging", testString);
        device.put("untethered_right_charging", testString);
        device.put("untethered_case_charging", testString);
        assertThat(db.insert("metadata", SQLiteDatabase.CONFLICT_IGNORE, device),
                CoreMatchers.not(-1));

        // Migrate database from 103 to 104
        db.close();
        db = testHelper.runMigrationsAndValidate(DB_NAME, 104, true,
                MetadataDatabase.MIGRATION_103_104);
        Cursor cursor = db.query("SELECT * FROM metadata");

        assertHasColumn(cursor, "last_active_time", true);
        assertHasColumn(cursor, "is_active_a2dp_device", true);

        while (cursor.moveToNext()) {
            // Check the two new columns were added with their default values
            assertColumnIntData(cursor, "last_active_time", -1);
            assertColumnIntData(cursor, "is_active_a2dp_device", 0);
        }
    }

    /**
     * Helper function to check whether the database has the expected column
     */
    void assertHasColumn(Cursor cursor, String columnName, boolean hasColumn) {
        if (hasColumn) {
            assertThat(cursor.getColumnIndex(columnName), CoreMatchers.not(-1));
        } else {
            assertThat(cursor.getColumnIndex(columnName), CoreMatchers.is(-1));
        }
    }

    /**
     * Helper function to check whether the database has the expected value
     */
    void assertColumnIntData(Cursor cursor, String columnName, int value) {
        assertThat(cursor.getInt(cursor.getColumnIndex(columnName)), CoreMatchers.is(value));
    }

    /**
     * Helper function to check whether the column data type is BLOB
     */
    void assertColumnBlob(Cursor cursor, String columnName) {
        assertThat(cursor.getType(cursor.getColumnIndex(columnName)),
                CoreMatchers.is(Cursor.FIELD_TYPE_BLOB));
    }

    /**
     * Helper function to check the BLOB data in a column is expected
     */
    void assertColumnBlobData(Cursor cursor, String columnName, byte[] data) {
        assertThat(cursor.getBlob(cursor.getColumnIndex(columnName)),
                CoreMatchers.is(data));
    }

    void restartDatabaseManagerHelper() {
        Metadata data = new Metadata(LOCAL_STORAGE);
        data.migrated = true;
        mDatabase.insert(data);

        mDatabaseManager.cleanup();
        mDatabaseManager.start(mDatabase);
        // Wait for handler thread finish its task.
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        // Remove local storage
        mDatabaseManager.mMetadataCache.remove(LOCAL_STORAGE);
        mDatabaseManager.deleteDatabase(data);
        // Wait for handler thread finish its task.
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    void testSetGetProfileConnectionPolicyCase(boolean stored, int connectionPolicy,
            int expectedConnectionPolicy, boolean expectedSetResult) {
        if (stored) {
            Metadata data = new Metadata(TEST_BT_ADDR);
            mDatabaseManager.mMetadataCache.put(TEST_BT_ADDR, data);
            mDatabase.insert(data);
        }
        Assert.assertEquals(expectedSetResult,
                mDatabaseManager.setProfileConnectionPolicy(mTestDevice,
                BluetoothProfile.HEADSET, connectionPolicy));
        Assert.assertEquals(expectedConnectionPolicy,
                mDatabaseManager.getProfileConnectionPolicy(mTestDevice, BluetoothProfile.HEADSET));
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        List<Metadata> list = mDatabase.load();

        // Check number of metadata in the database
        if (!stored) {
            if (connectionPolicy != BluetoothProfile.CONNECTION_POLICY_FORBIDDEN
                    && connectionPolicy != BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
                // Database won't be updated
                Assert.assertEquals(0, list.size());
                return;
            }
        }
        Assert.assertEquals(1, list.size());

        // Check whether the device is in database
        restartDatabaseManagerHelper();
        Assert.assertEquals(expectedConnectionPolicy,
                mDatabaseManager.getProfileConnectionPolicy(mTestDevice, BluetoothProfile.HEADSET));

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    void testSetGetA2dpOptionalCodecsCase(int test, boolean stored, int value, int expectedValue) {
        if (stored) {
            Metadata data = new Metadata(TEST_BT_ADDR);
            mDatabaseManager.mMetadataCache.put(TEST_BT_ADDR, data);
            mDatabase.insert(data);
        }
        if (test == A2DP_SUPPORT_OP_CODEC_TEST) {
            mDatabaseManager.setA2dpSupportsOptionalCodecs(mTestDevice, value);
            Assert.assertEquals(expectedValue,
                    mDatabaseManager.getA2dpSupportsOptionalCodecs(mTestDevice));
        } else {
            mDatabaseManager.setA2dpOptionalCodecsEnabled(mTestDevice, value);
            Assert.assertEquals(expectedValue,
                    mDatabaseManager.getA2dpOptionalCodecsEnabled(mTestDevice));
        }
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        List<Metadata> list = mDatabase.load();

        // Check number of metadata in the database
        if (!stored) {
            // Database won't be updated
            Assert.assertEquals(0, list.size());
            return;
        }
        Assert.assertEquals(1, list.size());

        // Check whether the device is in database
        restartDatabaseManagerHelper();
        if (test == A2DP_SUPPORT_OP_CODEC_TEST) {
            Assert.assertEquals(expectedValue,
                    mDatabaseManager.getA2dpSupportsOptionalCodecs(mTestDevice));
        } else {
            Assert.assertEquals(expectedValue,
                    mDatabaseManager.getA2dpOptionalCodecsEnabled(mTestDevice));
        }

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }

    void testSetGetCustomMetaCase(boolean stored, int key, byte[] value, boolean expectedResult) {
        byte[] testValue = "test value".getBytes();
        int verifyTime = 1;
        if (stored) {
            Metadata data = new Metadata(TEST_BT_ADDR);
            mDatabaseManager.mMetadataCache.put(TEST_BT_ADDR, data);
            mDatabase.insert(data);
            Assert.assertEquals(expectedResult,
                    mDatabaseManager.setCustomMeta(mTestDevice, key, testValue));
            verify(mAdapterService).metadataChanged(TEST_BT_ADDR, key, testValue);
            verifyTime++;
        }
        Assert.assertEquals(expectedResult,
                mDatabaseManager.setCustomMeta(mTestDevice, key, value));
        if (expectedResult) {
            // Check for callback and get value
            verify(mAdapterService, times(verifyTime)).metadataChanged(TEST_BT_ADDR, key, value);
            Assert.assertEquals(value,
                    mDatabaseManager.getCustomMeta(mTestDevice, key));
        } else {
            Assert.assertNull(mDatabaseManager.getCustomMeta(mTestDevice, key));
            return;
        }
        // Wait for database update
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());

        // Check whether the value is saved in database
        restartDatabaseManagerHelper();
        Assert.assertArrayEquals(value,
                mDatabaseManager.getCustomMeta(mTestDevice, key));

        mDatabaseManager.factoryReset();
        mDatabaseManager.mMetadataCache.clear();
        // Wait for clear database
        TestUtils.waitForLooperToFinishScheduledTask(mDatabaseManager.getHandlerLooper());
    }
}
