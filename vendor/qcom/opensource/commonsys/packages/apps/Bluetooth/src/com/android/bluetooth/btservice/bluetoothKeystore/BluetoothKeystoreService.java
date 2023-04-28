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

import android.annotation.Nullable;
import android.os.SystemProperties;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Log;

import com.android.bluetooth.BluetoothKeystoreProto;
import com.android.internal.annotations.VisibleForTesting;

import com.google.protobuf.ByteString;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.ProviderException;
import java.security.UnrecoverableEntryException;
import java.security.cert.CertificateException;
import java.util.Base64;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.KeyGenerator;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * Service used for handling encryption and decryption of the bt_config.conf
 */
public class BluetoothKeystoreService {
    private static final String TAG = "BluetoothKeystoreService";

    private static final boolean DBG = false;

    private static BluetoothKeystoreService sBluetoothKeystoreService;
    private boolean mCleaningUp;
    private boolean mIsCommonCriteriaMode;

    private static final String CIPHER_ALGORITHM = "AES/GCM/NoPadding";
    private static final int GCM_TAG_LENGTH = 128;
    private static final int KEY_LENGTH = 256;
    private static final String KEYALIAS = "bluetooth-key-encrypted";
    private static final String KEY_STORE = "AndroidKeyStore";
    private static final int TRY_MAX = 3;

    private static final String CONFIG_FILE_PREFIX = "bt_config-origin";
    private static final String CONFIG_BACKUP_PREFIX = "bt_config-backup";

    private static final String CONFIG_FILE_HASH = "hash";

    private static final String CONFIG_CHECKSUM_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.checksum.encrypted";
    private static final String CONFIG_FILE_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.conf.encrypted";
    private static final String CONFIG_BACKUP_ENCRYPTION_PATH =
            "/data/misc/bluedroid/bt_config.bak.encrypted";

    private static final String CONFIG_FILE_PATH = "/data/misc/bluedroid/bt_config.conf";
    private static final String CONFIG_BACKUP_PATH = "/data/misc/bluedroid/bt_config.bak";
    private static final String CONFIG_FILE_CHECKSUM_PATH =
            "/data/misc/bluedroid/bt_config.conf.encrypted-checksum";
    private static final String CONFIG_BACKUP_CHECKSUM_PATH =
            "/data/misc/bluedroid/bt_config.bak.encrypted-checksum";

    private static final int BUFFER_SIZE = 400 * 10;

    private static final int CONFIG_COMPARE_INIT = 0b00;
    private static final int CONFIG_FILE_COMPARE_PASS = 0b01;
    private static final int CONFIG_BACKUP_COMPARE_PASS = 0b10;
    private int mCompareResult;

    BluetoothKeystoreNativeInterface mBluetoothKeystoreNativeInterface;

    private ComputeDataThread mEncryptDataThread;
    private ComputeDataThread mDecryptDataThread;
    private Map<String, String> mNameEncryptKey = new HashMap<>();
    private Map<String, String> mNameDecryptKey = new HashMap<>();
    private BlockingQueue<String> mPendingDecryptKey = new LinkedBlockingQueue<>();
    private BlockingQueue<String> mPendingEncryptKey = new LinkedBlockingQueue<>();
    private final List<String> mEncryptKeyNameList = List.of("LinkKey", "LE_KEY_PENC", "LE_KEY_PID",
            "LE_KEY_LID", "LE_KEY_PCSRK", "LE_KEY_LENC", "LE_KEY_LCSRK");

    private Base64.Decoder mDecoder = Base64.getDecoder();
    private Base64.Encoder mEncoder = Base64.getEncoder();

    public BluetoothKeystoreService(boolean isCommonCriteriaMode) {
        debugLog("new BluetoothKeystoreService isCommonCriteriaMode: " + isCommonCriteriaMode);
        mIsCommonCriteriaMode = isCommonCriteriaMode;
        mCompareResult = CONFIG_COMPARE_INIT;
        startThread();
    }

    /**
     * Start and initialize the BluetoothKeystoreService
     */
    public void start() {
        debugLog("start");
        KeyStore keyStore;

        if (sBluetoothKeystoreService != null) {
            errorLog("start() called twice");
            return;
        }

        keyStore = getKeyStore();

        // Confirm whether to enable Common Criteria mode for the first time.
        if (keyStore == null) {
            debugLog("cannot find the keystore.");
            return;
        }

        mBluetoothKeystoreNativeInterface = Objects.requireNonNull(
                BluetoothKeystoreNativeInterface.getInstance(),
                "BluetoothKeystoreNativeInterface cannot be null when BluetoothKeystore starts");

        // Mark service as started
        setBluetoothKeystoreService(this);

        try {
            if (!keyStore.containsAlias(KEYALIAS) && mIsCommonCriteriaMode) {
                infoLog("Enable Common Criteria mode for the first time, pass hash check.");
                mCompareResult = 0b11;
                return;
            }
        } catch (KeyStoreException e) {
            reportKeystoreException(e, "cannot find the keystore");
            return;
        }

        loadConfigData();
    }

    /**
     * Factory reset the keystore service.
     */
    public void factoryReset() {
        try {
            cleanupAll();
        } catch (IOException e) {
            reportBluetoothKeystoreException(e, "IO error while file operating.");
        }
    }

    /**
     * Cleans up the keystore service.
     */
    public void cleanup() {
        debugLog("cleanup");
        if (mCleaningUp) {
            debugLog("already doing cleanup");
        }

        mCleaningUp = true;

        if (sBluetoothKeystoreService == null) {
            debugLog("cleanup() called before start()");
            return;
        }

        // Mark service as stopped
        setBluetoothKeystoreService(null);

        // Cleanup native interface
        mBluetoothKeystoreNativeInterface.cleanup();
        mBluetoothKeystoreNativeInterface = null;

        if (mIsCommonCriteriaMode) {
            cleanupForCommonCriteriaModeEnable();
        } else {
            cleanupForCommonCriteriaModeDisable();
        }
    }

    /**
     * Clean up if Common Criteria mode is enabled.
     */
    @VisibleForTesting
    public void cleanupForCommonCriteriaModeEnable() {
        try {
            Thread.sleep(100);
            setEncryptKeyOrRemoveKey(CONFIG_FILE_PREFIX, CONFIG_FILE_HASH);
        } catch (InterruptedException e) {
            reportBluetoothKeystoreException(e, "Interrupted while operating.");
        } catch (IOException e) {
            reportBluetoothKeystoreException(e, "IO error while file operating.");
        } catch (NoSuchAlgorithmException e) {
            reportBluetoothKeystoreException(e, "encrypt could not find the algorithm: SHA256");
        }
        cleanupMemory();
        stopThread();
    }

    /**
     * Clean up if Common Criteria mode is disabled.
     */
    @VisibleForTesting
    public void cleanupForCommonCriteriaModeDisable() {
        mNameDecryptKey.clear();
        mNameEncryptKey.clear();
    }

    /**
     * Load decryption data from file.
     */
    @VisibleForTesting
    public void loadConfigData() {
        try {
            debugLog("loadConfigData");

            if (isFactoryReset()) {
                cleanupAll();
            }

            if (Files.exists(Paths.get(CONFIG_CHECKSUM_ENCRYPTION_PATH))) {
                debugLog("Load encryption file.");
                // Step2: Load checksum file.
                loadEncryptionFile(CONFIG_CHECKSUM_ENCRYPTION_PATH, false);
                // Step3: Compare hash file.
                if (compareFileHash(CONFIG_FILE_PATH)) {
                    debugLog("bt_config.conf checksum pass.");
                    mCompareResult = mCompareResult | CONFIG_FILE_COMPARE_PASS;
                }
                if (compareFileHash(CONFIG_BACKUP_PATH)) {
                    debugLog("bt_config.bak checksum pass.");
                    mCompareResult = mCompareResult | CONFIG_BACKUP_COMPARE_PASS;
                }
                // Step4: choose which encryption file loads.
                if (doesComparePass(CONFIG_FILE_COMPARE_PASS)) {
                    loadEncryptionFile(CONFIG_FILE_ENCRYPTION_PATH, true);
                } else if (doesComparePass(CONFIG_BACKUP_COMPARE_PASS)) {
                    Files.deleteIfExists(Paths.get(CONFIG_FILE_ENCRYPTION_PATH));
                    mNameEncryptKey.remove(CONFIG_FILE_PREFIX);
                    loadEncryptionFile(CONFIG_BACKUP_ENCRYPTION_PATH, true);
                } else {
                    // if the Common Criteria mode is disable, don't show the log.
                    if (mIsCommonCriteriaMode) {
                        debugLog("Config file conf and bak checksum check fail.");
                    }
                    cleanupAll();
                    return;
                }
            }
            // keep memory data for get decrypted key if Common Criteria mode disable.
            if (!mIsCommonCriteriaMode) {
                stopThread();
                cleanupFile();
            }
        } catch (IOException e) {
            reportBluetoothKeystoreException(e, "IO error while file operating.");
        } catch (InterruptedException e) {
            reportBluetoothKeystoreException(e, "Interrupted while operating.");
        } catch (NoSuchAlgorithmException e) {
            reportBluetoothKeystoreException(e, "could not find the algorithm: SHA256");
        }
    }

    private boolean isFactoryReset() {
        return SystemProperties.getBoolean("persist.bluetooth.factoryreset", false);
    }

    /**
     * Init JNI
     */
    public void initJni() {
        debugLog("initJni()");
        // Need to make sure all keys are decrypted.
        stopThread();
        startThread();
        // Initialize native interface
        mBluetoothKeystoreNativeInterface.init();
    }

    private boolean isAvailable() {
        return !mCleaningUp;
    }

    /**
     * Get the BluetoothKeystoreService instance
     */
    public static synchronized BluetoothKeystoreService getBluetoothKeystoreService() {
        if (sBluetoothKeystoreService == null) {
            debugLog("getBluetoothKeystoreService(): service is NULL");
            return null;
        }

        if (!sBluetoothKeystoreService.isAvailable()) {
            debugLog("getBluetoothKeystoreService(): service is not available");
            return null;
        }
        return sBluetoothKeystoreService;
    }

    private static synchronized void setBluetoothKeystoreService(
            BluetoothKeystoreService instance) {
        debugLog("setBluetoothKeystoreService(): set to: " + instance);
        sBluetoothKeystoreService = instance;
    }

    /**
     * Gets result of the checksum comparison
     */
    public int getCompareResult() {
        debugLog("getCompareResult: " + mCompareResult);
        return mCompareResult;
    }

    /**
     * Sets or removes the encryption key value.
     *
     * <p>If the value of decryptedString matches {@link #CONFIG_FILE_HASH} then
     * read the hash file and decrypt the keys and place them into {@link mPendingEncryptKey}
     * otherwise cleanup all data and remove the keys.
     *
     * @param prefixString key to use
     * @param decryptedString string to decrypt
     */
    public void setEncryptKeyOrRemoveKey(String prefixString, String decryptedString)
            throws InterruptedException, IOException, NoSuchAlgorithmException {
        infoLog("setEncryptKeyOrRemoveKey: prefix: " + prefixString);
        if (prefixString == null || decryptedString == null) {
            return;
        }
        if (prefixString.equals(CONFIG_FILE_PREFIX)) {
            if (decryptedString.isEmpty()) {
                cleanupAll();
            } else if (decryptedString.equals(CONFIG_FILE_HASH)) {
                backupConfigEncryptionFile();
                readHashFile(CONFIG_FILE_PATH, CONFIG_FILE_PREFIX);
                //save Map
                if (mNameDecryptKey.containsKey(CONFIG_FILE_PREFIX)
                        && mNameDecryptKey.get(CONFIG_FILE_PREFIX).equals(
                        mNameDecryptKey.get(CONFIG_BACKUP_PREFIX))) {
                    infoLog("Since the hash is same with previous, don't need encrypt again.");
                } else {
                    mPendingEncryptKey.put(prefixString);
                }
                saveEncryptedKey();
            }
            return;
        }

        if (decryptedString.isEmpty()) {
            // clear the item by prefixString.
            mNameDecryptKey.remove(prefixString);
            mNameEncryptKey.remove(prefixString);
        } else {
            mNameDecryptKey.put(prefixString, decryptedString);
            mPendingEncryptKey.put(prefixString);
        }
    }

    /**
     * Clean up memory and all files.
     */
    @VisibleForTesting
    public void cleanupAll() throws IOException {
        cleanupFile();
        cleanupMemory();
    }

    private void cleanupFile() throws IOException {
        Files.deleteIfExists(Paths.get(CONFIG_CHECKSUM_ENCRYPTION_PATH));
        Files.deleteIfExists(Paths.get(CONFIG_FILE_ENCRYPTION_PATH));
        Files.deleteIfExists(Paths.get(CONFIG_BACKUP_ENCRYPTION_PATH));
    }

    /**
     * Clean up memory.
     */
    @VisibleForTesting
    public void cleanupMemory() {
        stopThread();
        mNameEncryptKey.clear();
        mNameDecryptKey.clear();
        startThread();
    }

    /**
     * Stop encrypt/decrypt thread.
     */
    @VisibleForTesting
    public void stopThread() {
        try {
            if (mEncryptDataThread != null) {
                mEncryptDataThread.setWaitQueueEmptyForStop();
                mEncryptDataThread.join();
            }
            if (mDecryptDataThread != null) {
                mDecryptDataThread.setWaitQueueEmptyForStop();
                mDecryptDataThread.join();
            }
        } catch (InterruptedException e) {
            reportBluetoothKeystoreException(e, "Interrupted while operating.");
        }
    }

    private void startThread() {
        mEncryptDataThread = new ComputeDataThread(true);
        mDecryptDataThread = new ComputeDataThread(false);
        mEncryptDataThread.start();
        mDecryptDataThread.start();
    }

    /**
     * Get key value from the mNameDecryptKey.
     */
    public String getKey(String prefixString) {
        infoLog("getKey: prefix: " + prefixString);
        if (!mNameDecryptKey.containsKey(prefixString)) {
            return null;
        }

        return mNameDecryptKey.get(prefixString);
    }

    /**
     * Save encryption key into the encryption file.
     */
    @VisibleForTesting
    public void saveEncryptedKey() {
        stopThread();
        List<String> configEncryptedLines = new LinkedList<>();
        List<String> keyEncryptedLines = new LinkedList<>();
        for (String key : mNameEncryptKey.keySet()) {
            if (key.equals(CONFIG_FILE_PREFIX) || key.equals(CONFIG_BACKUP_PREFIX)) {
                configEncryptedLines.add(getEncryptedKeyData(key));
            } else {
                keyEncryptedLines.add(getEncryptedKeyData(key));
            }
        }
        startThread();

        try {
            if (!configEncryptedLines.isEmpty()) {
                Files.write(Paths.get(CONFIG_CHECKSUM_ENCRYPTION_PATH), configEncryptedLines);
            }
            if (!keyEncryptedLines.isEmpty()) {
                Files.write(Paths.get(CONFIG_FILE_ENCRYPTION_PATH), keyEncryptedLines);
            }
        } catch (IOException e) {
            throw new RuntimeException("write encryption file fail");
        }
    }

    private String getEncryptedKeyData(String prefixString) {
        if (prefixString == null) {
            return null;
        }
        return prefixString.concat("-").concat(mNameEncryptKey.get(prefixString));
    }

    /*
     * Get the mNameEncryptKey hashMap.
     */
    @VisibleForTesting
    public Map<String, String> getNameEncryptKey() {
        return mNameEncryptKey;
    }

    /*
     * Get the mNameDecryptKey hashMap.
     */
    @VisibleForTesting
    public Map<String, String> getNameDecryptKey() {
        return mNameDecryptKey;
    }

    private void backupConfigEncryptionFile() throws IOException {
        if (Files.exists(Paths.get(CONFIG_FILE_ENCRYPTION_PATH))) {
            Files.move(Paths.get(CONFIG_FILE_ENCRYPTION_PATH),
                    Paths.get(CONFIG_BACKUP_ENCRYPTION_PATH),
                    StandardCopyOption.REPLACE_EXISTING);
        }
        if (mNameEncryptKey.containsKey(CONFIG_FILE_PREFIX)) {
            mNameEncryptKey.put(CONFIG_BACKUP_PREFIX, mNameEncryptKey.get(CONFIG_FILE_PREFIX));
        }
        if (mNameDecryptKey.containsKey(CONFIG_FILE_PREFIX)) {
            mNameDecryptKey.put(CONFIG_BACKUP_PREFIX, mNameDecryptKey.get(CONFIG_FILE_PREFIX));
        }
    }

    private boolean doesComparePass(int item) {
        return (mCompareResult & item) == item;
    }

    /**
     * Compare config file checksum.
     */
    @VisibleForTesting
    public boolean compareFileHash(String hashFilePathString)
            throws InterruptedException, IOException, NoSuchAlgorithmException {
        if (!Files.exists(Paths.get(hashFilePathString))) {
            infoLog("compareFileHash: File does not exist, path: " + hashFilePathString);
            return false;
        }

        String prefixString = null;
        if (CONFIG_FILE_PATH.equals(hashFilePathString)) {
            prefixString = CONFIG_FILE_PREFIX;
        } else if (CONFIG_BACKUP_PATH.equals(hashFilePathString)) {
            prefixString = CONFIG_BACKUP_PREFIX;
        }
        if (prefixString == null) {
            errorLog("compareFileHash: Unexpected hash file path: " + hashFilePathString);
            return false;
        }

        readHashFile(hashFilePathString, prefixString);

        if (!mNameEncryptKey.containsKey(prefixString)) {
            errorLog("compareFileHash: NameEncryptKey doesn't contain the key, prefix:"
                    + prefixString);
            return false;
        }
        String encryptedData = mNameEncryptKey.get(prefixString);
        String decryptedData = tryCompute(encryptedData, false);
        if (decryptedData == null) {
            errorLog("compareFileHash: decrypt encrypted hash data fail, prefix: " + prefixString);
            return false;
        }

        return decryptedData.equals(mNameDecryptKey.get(prefixString));
    }

    private void readHashFile(String filePathString, String prefixString)
            throws InterruptedException, NoSuchAlgorithmException {
        byte[] dataBuffer = new byte[BUFFER_SIZE];
        int bytesRead  = 0;
        boolean successful = false;
        int counter = 0;
        while (!successful && counter < TRY_MAX) {
            try {
                MessageDigest messageDigest = MessageDigest.getInstance("SHA-256");
                InputStream fileStream = Files.newInputStream(Paths.get(filePathString));
                while ((bytesRead = fileStream.read(dataBuffer)) != -1) {
                    messageDigest.update(dataBuffer, 0, bytesRead);
                }

                try {
                    /* Closing the fileStream to avoid StrictMode */
                    fileStream.close();
                } catch (IOException e) {
                    /* Ignore if we cannot close the stream. */
                    debugLog("Fail to close the stream");
                }

                byte[] messageDigestBytes = messageDigest.digest();
                StringBuffer hashString = new StringBuffer();
                for (int index = 0; index < messageDigestBytes.length; index++) {
                    hashString.append(Integer.toString((
                            messageDigestBytes[index] & 0xff) + 0x100, 16).substring(1));
                }

                mNameDecryptKey.put(prefixString, hashString.toString());
                successful = true;
            } catch (IOException e) {
                infoLog("Fail to open file, try again. counter: " + counter);
                Thread.sleep(50);
                counter++;
            }
        }
        if (counter > 3) {
            errorLog("Fail to open file");
        }
    }

    private void readChecksumFile(String filePathString, String prefixString) throws IOException {
        if (!Files.exists(Paths.get(filePathString))) {
            return;
        }
        byte[] allBytes = Files.readAllBytes(Paths.get(filePathString));
        String checksumDataBase64 = mEncoder.encodeToString(allBytes);
        mNameEncryptKey.put(prefixString, checksumDataBase64);
    }

    /**
     * Parses a file to search for the key and put it into the pending compute queue
     */
    @VisibleForTesting
    public void parseConfigFile(String filePathString)
            throws IOException, InterruptedException {
        String prefixString = null;
        String dataString = null;
        String name = null;
        String key = null;
        int index;

        if (!Files.exists(Paths.get(filePathString))) {
            return;
        }
        List<String> allLinesString = Files.readAllLines(Paths.get(filePathString));
        for (String line : allLinesString) {
            if (line.startsWith("[")) {
                name = line.replace("[", "").replace("]", "");
                continue;
            }

            index = line.indexOf(" = ");
            if (index < 0) {
                continue;
            }
            key = line.substring(0, index);

            if (!mEncryptKeyNameList.contains(key)) {
                continue;
            }

            if (name == null) {
                continue;
            }

            prefixString = name + "-" + key;
            dataString = line.substring(index + 3);
            if (dataString.length() == 0) {
                continue;
            }

            mNameDecryptKey.put(prefixString, dataString);
            mPendingEncryptKey.put(prefixString);
        }
    }

    /**
     * Load encryption file and push into mNameEncryptKey and pendingDecryptKey.
     */
    @VisibleForTesting
    public void loadEncryptionFile(String filePathString, boolean doDecrypt)
            throws InterruptedException {
        try {
            if (!Files.exists(Paths.get(filePathString))) {
                return;
            }
            List<String> allLinesString = Files.readAllLines(Paths.get(filePathString));
            for (String line : allLinesString) {
                int index = line.lastIndexOf("-");
                if (index < 0) {
                    continue;
                }
                String prefixString = line.substring(0, index);
                String encryptedString = line.substring(index + 1);

                mNameEncryptKey.put(prefixString, encryptedString);
                if (doDecrypt) {
                    mPendingDecryptKey.put(prefixString);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException("read encryption file all line fail");
        }
    }

    // will retry TRY_MAX times.
    private String tryCompute(String sourceData, boolean doEncrypt) {
        int counter = 0;
        String targetData = null;

        if (sourceData == null) {
            return null;
        }

        while (targetData == null && counter < TRY_MAX) {
            if (doEncrypt) {
                targetData = encrypt(sourceData);
            } else {
                targetData = decrypt(sourceData);
            }
            counter++;
        }
        return targetData;
    }

    /**
     * Encrypt the provided data blob.
     *
     * @param data String to be encrypted.
     * @return String as base64.
     */
    private @Nullable String encrypt(String data) {
        BluetoothKeystoreProto.EncryptedData protobuf;
        byte[] outputBytes;
        String outputBase64 = null;

        try {
            if (data == null) {
                errorLog("encrypt: data is null");
                return outputBase64;
            }
            Cipher cipher = Cipher.getInstance(CIPHER_ALGORITHM);
            SecretKey secretKeyReference = getOrCreateSecretKey();

            if (secretKeyReference != null) {
                cipher.init(Cipher.ENCRYPT_MODE, secretKeyReference);
                protobuf = BluetoothKeystoreProto.EncryptedData.newBuilder()
                    .setEncryptedData(ByteString.copyFrom(cipher.doFinal(data.getBytes())))
                    .setInitVector(ByteString.copyFrom(cipher.getIV())).build();

                outputBytes = protobuf.toByteArray();
                if (outputBytes == null) {
                    errorLog("encrypt: Failed to serialize EncryptedData protobuf.");
                    return outputBase64;
                }
                outputBase64 = mEncoder.encodeToString(outputBytes);
            } else {
                errorLog("encrypt: secretKeyReference is null.");
            }
        } catch (NoSuchAlgorithmException e) {
            reportKeystoreException(e, "encrypt could not find the algorithm: " + CIPHER_ALGORITHM);
        } catch (NoSuchPaddingException e) {
            reportKeystoreException(e, "encrypt had a padding exception");
        } catch (InvalidKeyException e) {
            reportKeystoreException(e, "encrypt received an invalid key");
        } catch (BadPaddingException e) {
            reportKeystoreException(e, "encrypt had a padding problem");
        } catch (IllegalBlockSizeException e) {
            reportKeystoreException(e, "encrypt had an illegal block size");
        }
        return outputBase64;
    }

    /**
     * Decrypt the original data blob from the provided {@link EncryptedData}.
     *
     * @param data String as base64 to be decrypted.
     * @return String.
     */
    private @Nullable String decrypt(String encryptedDataBase64) {
        BluetoothKeystoreProto.EncryptedData protobuf;
        byte[] encryptedDataBytes;
        byte[] decryptedDataBytes;
        String output = null;

        try {
            if (encryptedDataBase64 == null) {
                errorLog("decrypt: encryptedDataBase64 is null");
                return output;
            }
            encryptedDataBytes = mDecoder.decode(encryptedDataBase64);
            protobuf = BluetoothKeystoreProto.EncryptedData.parser().parseFrom(encryptedDataBytes);
            Cipher cipher = Cipher.getInstance(CIPHER_ALGORITHM);
            GCMParameterSpec spec =
                    new GCMParameterSpec(GCM_TAG_LENGTH, protobuf.getInitVector().toByteArray());
            SecretKey secretKeyReference = getOrCreateSecretKey();

            if (secretKeyReference != null) {
                cipher.init(Cipher.DECRYPT_MODE, secretKeyReference, spec);
                decryptedDataBytes = cipher.doFinal(protobuf.getEncryptedData().toByteArray());
                output = new String(decryptedDataBytes);
            } else {
                errorLog("decrypt: secretKeyReference is null.");
            }
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            reportBluetoothKeystoreException(e, "decrypt: Failed to parse EncryptedData protobuf.");
        } catch (NoSuchAlgorithmException e) {
            reportKeystoreException(e,
                    "decrypt could not find cipher algorithm " + CIPHER_ALGORITHM);
        } catch (NoSuchPaddingException e) {
            reportKeystoreException(e, "decrypt could not find padding algorithm");
        } catch (IllegalBlockSizeException e) {
            reportKeystoreException(e, "decrypt had a illegal block size");
        } catch (BadPaddingException e) {
            reportKeystoreException(e, "decrypt had bad padding");
        } catch (InvalidKeyException e) {
            reportKeystoreException(e, "decrypt had an invalid key");
        } catch (InvalidAlgorithmParameterException e) {
            reportKeystoreException(e, "decrypt had an invalid algorithm parameter");
        }
        return output;
    }

    private KeyStore getKeyStore() {
        KeyStore keyStore = null;
        int counter = 0;

        while ((counter <= TRY_MAX) && (keyStore == null)) {
            try {
                keyStore = KeyStore.getInstance("AndroidKeyStore");
                keyStore.load(null);
            } catch (KeyStoreException | CertificateException | NoSuchAlgorithmException
                    | IOException e) {
                reportKeystoreException(e, "cannot open keystore");
            }
            counter++;
        }
        return keyStore;
    }

    // The getOrGenerate semantic on keystore is not thread safe, need to synchronized it.
    private synchronized SecretKey getOrCreateSecretKey() {
        SecretKey secretKey = null;
        try {
            KeyStore keyStore = getKeyStore();
            if (keyStore.containsAlias(KEYALIAS)) { // The key exists in key store. Get the key.
                KeyStore.SecretKeyEntry secretKeyEntry = (KeyStore.SecretKeyEntry) keyStore
                        .getEntry(KEYALIAS, null);

                if (secretKeyEntry != null) {
                    secretKey = secretKeyEntry.getSecretKey();
                } else {
                    errorLog("decrypt: secretKeyEntry is null.");
                }
            } else {
                // The key does not exist in key store. Create the key and store it.
                KeyGenerator keyGenerator = KeyGenerator
                        .getInstance(KeyProperties.KEY_ALGORITHM_AES, KEY_STORE);

                KeyGenParameterSpec keyGenParameterSpec = new KeyGenParameterSpec.Builder(KEYALIAS,
                        KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                        .setKeySize(KEY_LENGTH)
                        .build();

                keyGenerator.init(keyGenParameterSpec);
                secretKey = keyGenerator.generateKey();
            }
        } catch (KeyStoreException e) {
            reportKeystoreException(e, "cannot find the keystore: " + KEY_STORE);
        } catch (InvalidAlgorithmParameterException e) {
            reportKeystoreException(e, "getOrCreateSecretKey had an invalid algorithm parameter");
        } catch (NoSuchAlgorithmException e) {
            reportKeystoreException(e, "getOrCreateSecretKey cannot find algorithm");
        } catch (NoSuchProviderException e) {
            reportKeystoreException(e, "getOrCreateSecretKey cannot find crypto provider");
        } catch (UnrecoverableEntryException e) {
            reportKeystoreException(e,
                    "getOrCreateSecretKey had an unrecoverable entry exception.");
        } catch (ProviderException e) {
            reportKeystoreException(e, "getOrCreateSecretKey had a provider exception.");
        }
        return secretKey;
    }

    private static void reportKeystoreException(Exception exception, String error) {
        Log.wtf(TAG, "A keystore error was encountered: " + error, exception);
    }

    private static void reportBluetoothKeystoreException(Exception exception, String error) {
        Log.wtf(TAG, "A bluetoothkey store error was encountered: " + error, exception);
    }

    private static void infoLog(String msg) {
        if (DBG) {
            Log.i(TAG, msg);
        }
    }

    private static void debugLog(String msg) {
        Log.d(TAG, msg);
    }

    private static void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    /**
     * A thread that decrypt data if the queue has new decrypt task.
     */
    private class ComputeDataThread extends Thread {
        private Map<String, String> mSourceDataMap;
        private Map<String, String> mTargetDataMap;
        private BlockingQueue<String> mSourceQueue;
        private boolean mDoEncrypt;

        private boolean mWaitQueueEmptyForStop;

        ComputeDataThread(boolean doEncrypt) {
            infoLog("ComputeDataThread: create, doEncrypt: " + doEncrypt);
            mWaitQueueEmptyForStop = false;
            mDoEncrypt = doEncrypt;

            if (mDoEncrypt) {
                mSourceDataMap = mNameDecryptKey;
                mTargetDataMap = mNameEncryptKey;
                mSourceQueue = mPendingEncryptKey;
            } else {
                mSourceDataMap = mNameEncryptKey;
                mTargetDataMap = mNameDecryptKey;
                mSourceQueue = mPendingDecryptKey;
            }
        }

        @Override
        public void run() {
            infoLog("ComputeDataThread: run, doEncrypt: " + mDoEncrypt);
            String prefixString;
            String sourceData;
            String targetData;
            while (!mSourceQueue.isEmpty() || !mWaitQueueEmptyForStop) {
                try {
                    prefixString = mSourceQueue.take();
                    if (mSourceDataMap.containsKey(prefixString)) {
                        sourceData = mSourceDataMap.get(prefixString);
                        targetData = tryCompute(sourceData, mDoEncrypt);
                        if (targetData != null) {
                            mTargetDataMap.put(prefixString, targetData);
                        } else {
                            errorLog("Computing of Data failed with prefixString: " + prefixString
                                    + ", doEncrypt: " + mDoEncrypt);
                        }
                    }
                } catch (InterruptedException e) {
                    infoLog("Interrupted while operating.");
                }
            }
            infoLog("ComputeDataThread: Stop, doEncrypt: " + mDoEncrypt);
        }

        public void setWaitQueueEmptyForStop() {
            mWaitQueueEmptyForStop = true;
            if (mPendingEncryptKey.isEmpty()) {
                interrupt();
            }
        }
    }
}
