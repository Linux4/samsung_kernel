/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.bluetooth.btservice.bluetoothkeystore;

import android.os.Binder;
import android.os.Process;
import android.util.Log;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class BluetoothKeystoreServiceTest {
    private static final String TAG = "BluetoothKeystoreServiceTest";
    private BluetoothKeystoreService mBluetoothKeystoreService;

    // Please also check bt_stack string configuration if you want to change the content.
    private static final String CONFIG_FILE_PREFIX = "bt_config-origin";
    private static final String CONFIG_BACKUP_PREFIX = "bt_config-backup";
    private static final String CONFIG_FILE_HASH = "hash";
    private static final String CONFIG_FILE_PATH = "/data/misc/bluedroid/bt_config.conf";
    private static final String CONFIG_FILE_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.conf.encrypted";
    private static final String CONFIG_BACKUP_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.bak.encrypted";
    private static final String CONFIG_CHECKSUM_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.checksum.encrypted";

    // bt_config file test content.
    private final List<String> mConfigTestData = List.of("[Info]",
            "FileSource = Empty",
            "TimeCreated = XXXX-XX-XX XX:XX:XX",
            "",
            "[Metrics]",
            "Salt256Bit = aaaaaaaaaaaaaaaaaaa",
            "",
            "[Adapter]",
            "Address = 11:22:33:44:55:66",
            "LE_LOCAL_KEY_IRK = IRK1234567890",
            "LE_LOCAL_KEY_IR = IR1234567890",
            "LE_LOCAL_KEY_DHK = DHK1234567890",
            "LE_LOCAL_KEY_ER = ER1234567890",
            "ScanMode = 0",
            "DiscoveryTimeout = 120",
            "",
            "[aa:bb:cc:dd:ee:ff]",
            "Timestamp = 12345678",
            "Name = Test",
            "DevClass = 1234567",
            "LinkKey = 11223344556677889900aabbccddeeff",
            "LE_KEY_PENC = ec111111111111111111111111111111111111111111111111111111",
            "LE_KEY_PID = d222222222222222222222222222222222222222222222",
            "LE_KEY_PCSRK = c33333333333333333333333333333333333333333333333",
            "LE_KEY_LENC = eec4444444444444444444444444444444444444",
            "LE_KEY_LCSRK = aec555555555555555555555555555555555555555555555",
            "LE_KEY_LID ="
            );

    private final List<String> mConfigTestDataForOldEncryptedFile = List.of("[Info]",
            "FileSource = Empty",
            "TimeCreated = XXXX-XX-XX XX:XX:XX",
            "",
            "[Metrics]",
            "Salt256Bit = aaaaaaaaaaaaaaaaaaa",
            "",
            "[Adapter]",
            "Address = 11:22:33:44:55:66",
            "LE_LOCAL_KEY_IRK = IRK1234567890",
            "LE_LOCAL_KEY_IR = IR1234567890",
            "LE_LOCAL_KEY_DHK = DHK1234567890",
            "LE_LOCAL_KEY_ER = ER1234567890",
            "ScanMode = 0",
            "DiscoveryTimeout = 120",
            "",
            "[aa:bb:cc:dd:ee:ff]",
            "Timestamp = 12345678",
            "Name = Test",
            "DevClass = 1234567",
            "LinkKey = CgzgWAk2ROa2cjknZhsaMIPzf20MvCRx2QeAHycQ7gFy9LnVi9FYs/PodOfl+FP5YkXP/WHkY4",
            "LE_KEY_PENC = CgwomFvDc/IuhOeoTn8aSPHqDneIZJ7aszhKQPorqeDPF50cytW4I/LzmdNvMeVX0qBsuh",
            "LE_KEY_PID = Cgxu7Z7sXniNj3ija1waPkvHCLH4gnttOyb0OZjiJj+xH3KvtfDh6k2wbgGcGTLe9pYS4EX",
            "LE_KEY_PCSRK = Cgx5t1PkIm8ohz9BLzYaQOsrmZakN77CMgbWeBIqT8bW6bQhK1JZYpp3qWZVM8HM3y09h",
            "LE_KEY_LENC = CgytzELdVX+QptdzuuMaOLDiNuzn7BNK9OmPNHYspp4ojThTA/5iWBxrZV6E3qZydLyNHk",
            "LE_KEY_LCSRK = CgydDaLIr/pSx1/eoPEaQEZN2BDpSJPjOSiJWwDBkMkgIpf/YmfxB6rUB8EXHkC+9eSy4",
            "LE_KEY_LID ="
            );

    private List<String> mConfigData = new LinkedList<>();

    private Map<String, String> mNameDecryptKeyResult = new HashMap<>();

    @Before
    public void setUp() {
        Assume.assumeTrue("Ignore test when the user is not primary.", isPrimaryUser());
        mBluetoothKeystoreService = new BluetoothKeystoreService(true);
        Assert.assertNotNull(mBluetoothKeystoreService);
        // backup origin config data.
        try {
            mConfigData = Files.readAllLines(Paths.get(CONFIG_FILE_PATH));
        } catch (IOException e) {
            Log.wtf(TAG, "Read file fail", e);
        }
        // create a mNameDecryptKeyResult for comparing.
        createNameDecryptKeyResult();
    }

    @After
    public void tearDown() {
        if (!isPrimaryUser()) {
            return;
        }
        try {
            if (!mConfigData.isEmpty()) {
                Files.write(Paths.get(CONFIG_FILE_PATH), mConfigData);
            }
            mBluetoothKeystoreService.cleanupAll();
        } catch (IOException e) {
            Log.wtf(TAG, "Write back file fail or clean up encryption file", e);
        }
        mBluetoothKeystoreService.stopThread();
        mBluetoothKeystoreService = null;
    }

    private boolean isPrimaryUser() {
        return Binder.getCallingUid() == Process.BLUETOOTH_UID;
    }

    private void overwriteConfigFile(List<String> data) {
        try {
            Files.write(Paths.get(CONFIG_FILE_PATH), data);
        } catch (IOException e) {
            Log.wtf(TAG, "Write file fail", e);
        }
    }

    private void createNameDecryptKeyResult() {
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LinkKey",
                "11223344556677889900aabbccddeeff");
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LE_KEY_PENC",
                "ec111111111111111111111111111111111111111111111111111111");
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LE_KEY_PID",
                "d222222222222222222222222222222222222222222222");
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LE_KEY_PCSRK",
                "c33333333333333333333333333333333333333333333333");
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LE_KEY_LENC",
                "eec4444444444444444444444444444444444444");
        mNameDecryptKeyResult.put("aa:bb:cc:dd:ee:ff-LE_KEY_LCSRK",
                "aec555555555555555555555555555555555555555555555");
    }

    private boolean doCompareKeySet(Map<String, String> map1, Map<String, String> map2) {
        return map1.keySet().equals(map2.keySet());
    }

    private boolean doCompareMap(Map<String, String> map1, Map<String, String> map2) {
        return map1.equals(map2);
    }

    private boolean parseConfigFile(String filePathString) {
        try {
            mBluetoothKeystoreService.parseConfigFile(filePathString);
            return true;
        } catch (IOException | InterruptedException e) {
            return false;
        }
    }

    private boolean loadEncryptionFile(String filePathString, boolean doDecrypt) {
        try {
            mBluetoothKeystoreService.loadEncryptionFile(filePathString, doDecrypt);
            return true;
        } catch (InterruptedException e) {
            return false;
        }
    }

    private boolean setEncryptKeyOrRemoveKey(String prefixString, String decryptedString) {
        try {
            mBluetoothKeystoreService.setEncryptKeyOrRemoveKey(prefixString, decryptedString);
            return true;
        } catch (InterruptedException | IOException | NoSuchAlgorithmException e) {
            return false;
        }
    }

    private boolean compareFileHash(String hashFilePathString) {
        try {
            return mBluetoothKeystoreService.compareFileHash(hashFilePathString);
        } catch (InterruptedException | IOException | NoSuchAlgorithmException e) {
            return false;
        }
    }

    @Test
    public void testParserFile() {
        // over write config
        overwriteConfigFile(mConfigTestData);
        // load config file.
        Assert.assertTrue(parseConfigFile(CONFIG_FILE_PATH));
        // make sure it is same with createNameDecryptKeyResult
        Assert.assertTrue(doCompareMap(mNameDecryptKeyResult,
                mBluetoothKeystoreService.getNameDecryptKey()));
    }

    @Test
    public void testEncrypt() {
        // load config file and put the unencrypted key in to queue.
        testParserFile();
        // Wait for encryption to complete
        mBluetoothKeystoreService.stopThread();

        Assert.assertTrue(doCompareKeySet(mNameDecryptKeyResult,
                mBluetoothKeystoreService.getNameEncryptKey()));
    }

    @Test
    public void testDecrypt() {
        // create an encrypted key list and save it.
        testEncrypt();
        mBluetoothKeystoreService.saveEncryptedKey();
        // clear up memory.
        mBluetoothKeystoreService.cleanupMemory();
        // load encryption file and do encryption.
        Assert.assertTrue(loadEncryptionFile(CONFIG_FILE_ENCRYPTION_PATH, true));
        // Wait for encryption to complete
        mBluetoothKeystoreService.stopThread();

        Assert.assertTrue(doCompareMap(mNameDecryptKeyResult,
                mBluetoothKeystoreService.getNameDecryptKey()));
    }

    @Test
    public void testCompareHashFile() {
        // save config checksum.
        Assert.assertTrue(setEncryptKeyOrRemoveKey(CONFIG_FILE_PREFIX, CONFIG_FILE_HASH));
        // clean up memory
        mBluetoothKeystoreService.cleanupMemory();

        Assert.assertTrue(loadEncryptionFile(CONFIG_CHECKSUM_ENCRYPTION_PATH, false));

        Assert.assertTrue(compareFileHash(CONFIG_FILE_PATH));
    }

    @Test
    public void testParserFileAfterDisableCommonCriteriaMode() {
        // preconfiguration.
        // need to creat encrypted file.
        testParserFile();
        // created encrypted file
        Assert.assertTrue(setEncryptKeyOrRemoveKey(CONFIG_FILE_PREFIX, CONFIG_FILE_HASH));
        // clean up memory and stop thread.
        mBluetoothKeystoreService.cleanupForCommonCriteriaModeEnable();

        // new mBluetoothKeystoreService and the Common Criteria mode is false.
        mBluetoothKeystoreService = new BluetoothKeystoreService(false);
        Assert.assertNotNull(mBluetoothKeystoreService);

        mBluetoothKeystoreService.loadConfigData();

        // check encryption file clean up.
        Assert.assertFalse(Files.exists(Paths.get(CONFIG_CHECKSUM_ENCRYPTION_PATH)));
        Assert.assertFalse(Files.exists(Paths.get(CONFIG_FILE_ENCRYPTION_PATH)));
        Assert.assertFalse(Files.exists(Paths.get(CONFIG_BACKUP_ENCRYPTION_PATH)));

        // remove hash data avoid interfering result.
        mBluetoothKeystoreService.getNameDecryptKey().remove(CONFIG_FILE_PREFIX);
        mBluetoothKeystoreService.getNameDecryptKey().remove(CONFIG_BACKUP_PREFIX);

        Assert.assertTrue(doCompareMap(mNameDecryptKeyResult,
                mBluetoothKeystoreService.getNameDecryptKey()));
    }

    @Test
    public void testParserFileAfterDisableCommonCriteriaModeWhenEnableCommonCriteriaMode() {
        testParserFileAfterDisableCommonCriteriaMode();
        mBluetoothKeystoreService.cleanupForCommonCriteriaModeDisable();

        // new mBluetoothKeystoreService and the Common Criteria mode is true.
        mBluetoothKeystoreService = new BluetoothKeystoreService(true);
        mBluetoothKeystoreService.loadConfigData();

        Assert.assertTrue(mBluetoothKeystoreService.getCompareResult() == 0);
    }
}
