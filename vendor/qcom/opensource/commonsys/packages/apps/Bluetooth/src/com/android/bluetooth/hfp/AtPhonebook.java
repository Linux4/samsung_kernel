/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.bluetooth.hfp;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.CallLog.Calls;
import android.provider.ContactsContract.CommonDataKinds;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.PhoneLookup;
import android.provider.ContactsContract.Contacts;
import android.telephony.PhoneNumberUtils;
import android.util.Log;

import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.util.DevicePolicyUtils;
import com.android.bluetooth.util.GsmAlphabet;

import java.util.HashMap;

/**
 * Helper for managing phonebook presentation over AT commands
 * @hide
 */
public class AtPhonebook {
    private static final String TAG = "BluetoothAtPhonebook";
    private static final boolean DBG = false;

    /** The projection to use when querying the call log database in response
     *  to AT+CPBR for the MC, RC, and DC phone books (missed, received, and
     *   dialed calls respectively)
     */
    private static final String[] CALLS_PROJECTION = new String[]{
            Calls._ID, Calls.NUMBER, Calls.NUMBER_PRESENTATION
    };

    /** The projection to use when querying the contacts database in response
     *   to AT+CPBR for the ME phonebook (saved phone numbers).
     */
    private static final String[] PHONES_PROJECTION = new String[]{
            Phone._ID, Phone.DISPLAY_NAME, Phone.NUMBER, Phone.TYPE
    };

    /** Android supports as many phonebook entries as the flash can hold, but
     *  BT periphals don't. Limit the number we'll report. */
    private static final int MAX_PHONEBOOK_SIZE = 16384;

    private final String SIM_URI = "content://icc/adn";

    static final String[] SIM_PROJECTION = new String[] {
            Contacts.DISPLAY_NAME,
            CommonDataKinds.Phone.NUMBER,
            CommonDataKinds.Phone.TYPE,
            CommonDataKinds.Phone.LABEL
    };

    private static final int NAME_COLUMN_INDEX = 0;
    private static final int NUMBER_COLUMN_INDEX = 1;
    private static final int NUMBERTYPE_COLUMN_INDEX = 2;

    private static final String OUTGOING_CALL_WHERE = Calls.TYPE + "=" + Calls.OUTGOING_TYPE;
    private static final String INCOMING_CALL_WHERE = Calls.TYPE + "=" + Calls.INCOMING_TYPE;
    private static final String MISSED_CALL_WHERE = Calls.TYPE + "=" + Calls.MISSED_TYPE;
    private static final String VISIBLE_PHONEBOOK_WHERE = null;
    private static final String VISIBLE_SIM_PHONEBOOK_WHERE = null;

    public static final int OUTGOING_IMS_TYPE = 1001;
    public static final int OUTGOING_WIFI_TYPE = 1004;

    private class PhonebookResult {
        public Cursor cursor; // result set of last query
        public int numberColumn;
        public int numberPresentationColumn;
        public int typeColumn;
        public int nameColumn;
    }

    private Context mContext;
    private ContentResolver mContentResolver;
    private HeadsetNativeInterface mNativeInterface;
    private String mCurrentPhonebook;
    private String mCharacterSet = "UTF-8";

    private int mCpbrIndex1, mCpbrIndex2;
    private boolean mCheckingAccessPermission;

    // package and class name to which we send intent to check phone book access permission
    private final String mPairingPackage;

    private final HashMap<String, PhonebookResult> mPhonebooks =
            new HashMap<String, PhonebookResult>(5);

    static final int TYPE_UNKNOWN = -1;
    static final int TYPE_READ = 0;
    static final int TYPE_SET = 1;
    static final int TYPE_TEST = 2;

    public AtPhonebook(Context context, HeadsetNativeInterface nativeInterface) {
        mContext = context;
        mPairingPackage = context.getString(R.string.pairing_ui_package);
        mContentResolver = context.getContentResolver();
        mNativeInterface = nativeInterface;
        mPhonebooks.put("DC", new PhonebookResult());  // dialled calls
        mPhonebooks.put("RC", new PhonebookResult());  // received calls
        mPhonebooks.put("MC", new PhonebookResult());  // missed calls
        mPhonebooks.put("ME", new PhonebookResult());  // mobile phonebook
        mPhonebooks.put("SM", new PhonebookResult());  // SIM phonebook

        mCurrentPhonebook = "ME";  // default to mobile phonebook
        mCpbrIndex1 = mCpbrIndex2 = -1;
    }

    public void cleanup() {
        mPhonebooks.clear();
    }

    /** Returns the last dialled number, or null if no numbers have been called */
    public String getLastDialledNumber() {
        String[] projection = {Calls.NUMBER};
        try {
            Bundle queryArgs = new Bundle();
            queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION,
                    Calls.TYPE + " = " + Calls.OUTGOING_TYPE + " OR " + Calls.TYPE +
                    " = " + OUTGOING_IMS_TYPE + " OR " + Calls.TYPE + " = " +
                    OUTGOING_WIFI_TYPE);
            queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SORT_ORDER, Calls.DEFAULT_SORT_ORDER);
            queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, 1);

            Cursor cursor = mContentResolver.query(Calls.CONTENT_URI, projection, queryArgs, null);
            log("Queried the last dialled number for CS, IMS, WIFI calls");
            if (cursor == null) {
                Log.w(TAG, "getLastDialledNumber, cursor is null");
                return null;
            }

            if (cursor.getCount() < 1) {
                cursor.close();
                Log.w(TAG, "getLastDialledNumber, cursor.getCount is 0");
                return null;
            }
            cursor.moveToNext();
            int column = cursor.getColumnIndexOrThrow(Calls.NUMBER);
            String number = cursor.getString(column);
            cursor.close();
            return number;
        } catch (Exception e) {
            Log.e(TAG, "Exception while querying last dialled number", e);
        }
        return null;
    }

    public boolean getCheckingAccessPermission() {
        return mCheckingAccessPermission;
    }

    public void setCheckingAccessPermission(boolean checkingAccessPermission) {
        mCheckingAccessPermission = checkingAccessPermission;
    }

    public void setCpbrIndex(int cpbrIndex) {
        mCpbrIndex1 = mCpbrIndex2 = cpbrIndex;
    }

    private byte[] getByteAddress(BluetoothDevice device) {
        return Utils.getBytesFromAddress(device.getAddress());
    }

    public void handleCscsCommand(String atString, int type, BluetoothDevice device) {
        log("handleCscsCommand - atString = " + atString);
        // Select Character Set
        int atCommandResult = HeadsetHalConstants.AT_RESPONSE_ERROR;
        int atCommandErrorCode = -1;
        String atCommandResponse = null;
        switch (type) {
            case TYPE_READ: // Read
                log("handleCscsCommand - Read Command");
                atCommandResponse = "+CSCS: \"" + mCharacterSet + "\"";
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                break;
            case TYPE_TEST: // Test
                log("handleCscsCommand - Test Command");
                atCommandResponse = ("+CSCS: (\"UTF-8\",\"IRA\",\"GSM\")");
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                break;
            case TYPE_SET: // Set
                log("handleCscsCommand - Set Command");
                String[] args = atString.split("=");
                if (args.length < 2 || args[1] == null) {
                    mNativeInterface.atResponseCode(device, atCommandResult, atCommandErrorCode);
                    break;
                }
                String characterSet = ((atString.split("="))[1]);
                characterSet = characterSet.replace("\"", "");
                if (characterSet.equals("GSM") || characterSet.equals("IRA") || characterSet.equals(
                        "UTF-8") || characterSet.equals("UTF8")) {
                    mCharacterSet = characterSet;
                    atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                } else {
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_SUPPORTED;
                }
                break;
            case TYPE_UNKNOWN:
            default:
                log("handleCscsCommand - Invalid chars");
                atCommandErrorCode = BluetoothCmeError.TEXT_HAS_INVALID_CHARS;
        }
        if (atCommandResponse != null) {
            mNativeInterface.atResponseString(device, atCommandResponse);
        }
        mNativeInterface.atResponseCode(device, atCommandResult, atCommandErrorCode);
    }

    public void handleCpbsCommand(String atString, int type, BluetoothDevice device) {
        // Select PhoneBook memory Storage
        log("handleCpbsCommand - atString = " + atString);
        int atCommandResult = HeadsetHalConstants.AT_RESPONSE_ERROR;
        int atCommandErrorCode = -1;
        String atCommandResponse = null;
        switch (type) {
            case TYPE_READ: // Read
                log("handleCpbsCommand - read command");
                // Return current size and max size
                PhonebookResult pbr = getPhonebookResult(mCurrentPhonebook, true);
                if (pbr == null) {
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_SUPPORTED;
                    break;
                }
                int size = pbr.cursor.getCount();
                atCommandResponse =
                        "+CPBS: \"" + mCurrentPhonebook + "\"," + size + "," + getMaxPhoneBookSize(
                                size);
                pbr.cursor.close();
                pbr.cursor = null;
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                break;
            case TYPE_TEST: // Test
                log("handleCpbsCommand - test command");
                atCommandResponse = ("+CPBS: (\"ME\",\"SM\",\"DC\",\"RC\",\"MC\")");
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                break;
            case TYPE_SET: // Set
                log("handleCpbsCommand - set command");
                String[] args = atString.split("=");
                // Select phonebook memory
                if (args.length < 2 || args[1] == null) {
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_SUPPORTED;
                    break;
                }
                String pb = args[1].trim();
                while (pb.endsWith("\"")) {
                    pb = pb.substring(0, pb.length() - 1);
                }
                while (pb.startsWith("\"")) {
                    pb = pb.substring(1, pb.length());
                }
                if (getPhonebookResult(pb, false) == null && !"SM".equals(pb)) {
                    if (DBG) {
                        log("Dont know phonebook: '" + pb + "'");
                    }
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_ALLOWED;
                    break;
                }
                mCurrentPhonebook = pb;
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                break;
            case TYPE_UNKNOWN:
            default:
                log("handleCpbsCommand - invalid chars");
                atCommandErrorCode = BluetoothCmeError.TEXT_HAS_INVALID_CHARS;
        }
        if (atCommandResponse != null) {
            mNativeInterface.atResponseString(device, atCommandResponse);
        }
        mNativeInterface.atResponseCode(device, atCommandResult, atCommandErrorCode);
    }

    void handleCpbrCommand(String atString, int type, BluetoothDevice remoteDevice) {
        log("handleCpbrCommand - atString = " + atString);
        int atCommandResult = HeadsetHalConstants.AT_RESPONSE_ERROR;
        int atCommandErrorCode = -1;
        String atCommandResponse = null;
        switch (type) {
            case TYPE_TEST: // Test
                /* Ideally we should return the maximum range of valid index's
                 * for the selected phone book, but this causes problems for the
                 * Parrot CK3300. So instead send just the range of currently
                 * valid index's.
                 */
                log("handleCpbrCommand - test command");
                int size;
                PhonebookResult pbr = getPhonebookResult(mCurrentPhonebook, true); //false);
                if (pbr == null) {
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_ALLOWED;
                    mNativeInterface.atResponseCode(remoteDevice, atCommandResult,
                            atCommandErrorCode);
                    break;
                }
                size = pbr.cursor.getCount();
                log("handleCpbrCommand - size = "+size);
                pbr.cursor.close();
                pbr.cursor = null;
                if (size == 0) {
                    /* Sending "+CPBR: (1-0)" can confused some carkits, send "1-1" * instead */
                    size = 1;
                }
                atCommandResponse = "+CPBR: (1-" + size + "),30,30";
                atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
                mNativeInterface.atResponseString(remoteDevice, atCommandResponse);
                mNativeInterface.atResponseCode(remoteDevice, atCommandResult, atCommandErrorCode);
                break;
            // Read PhoneBook Entries
            case TYPE_READ:
            case TYPE_SET: // Set & read
                // Phone Book Read Request
                // AT+CPBR=<index1>[,<index2>]
                log("handleCpbrCommand - set/read command");
                if (mCpbrIndex1 != -1) {
                   Log.i(TAG, "mCpbrIndex1 :" + mCpbrIndex1);
                   /* handling a CPBR at the moment, reject this CPBR command */
                    atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_ALLOWED;
                    mNativeInterface.atResponseCode(remoteDevice, atCommandResult,
                            atCommandErrorCode);
                    break;
                }
                // Parse indexes
                int index1;
                int index2;
                if ((atString.split("=")).length < 2) {
                    mNativeInterface.atResponseCode(remoteDevice, atCommandResult,
                            atCommandErrorCode);
                    break;
                }
                String atCommand = (atString.split("="))[1];
                String[] indices = atCommand.split(",");
                //replace AT command separator ';' from the index if any
                for (int i = 0; i < indices.length; i++) {
                    indices[i] = indices[i].replace(';', ' ').trim();
                }
                try {
                    index1 = Integer.parseInt(indices[0]);
                    if (indices.length == 1) {
                        index2 = index1;
                    } else {
                        index2 = Integer.parseInt(indices[1]);
                    }
                } catch (Exception e) {
                    log("handleCpbrCommand - exception - invalid chars: " + e.toString());
                    atCommandErrorCode = BluetoothCmeError.TEXT_HAS_INVALID_CHARS;
                    mNativeInterface.atResponseCode(remoteDevice, atCommandResult,
                            atCommandErrorCode);
                    break;
                }
                mCpbrIndex1 = index1;
                mCpbrIndex2 = index2;
                mCheckingAccessPermission = true;

                int permission = checkAccessPermission(remoteDevice);
                Log.i(TAG, "permission :" + permission);
                if (permission == BluetoothDevice.ACCESS_ALLOWED) {
                    Log.i(TAG, "permission to access is granted:" );
                    mCheckingAccessPermission = false;
                    atCommandResult = processCpbrCommand(remoteDevice);
                    mCpbrIndex1 = mCpbrIndex2 = -1;
                    mNativeInterface.atResponseCode(remoteDevice, atCommandResult,
                            atCommandErrorCode);
                    break;
                } else if (permission == BluetoothDevice.ACCESS_REJECTED) {
                    Log.i(TAG, "permission to access is not granted:" );
                    mCheckingAccessPermission = false;
                    mCpbrIndex1 = mCpbrIndex2 = -1;
                    mNativeInterface.atResponseCode(remoteDevice,
                            HeadsetHalConstants.AT_RESPONSE_ERROR, BluetoothCmeError.AG_FAILURE);
                }
                // If checkAccessPermission(remoteDevice) has returned
                // BluetoothDevice.ACCESS_UNKNOWN, we will continue the process in
                // HeadsetStateMachine.handleAccessPermissionResult(Intent) once HeadsetService
                // receives BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY from Settings app.
                break;
            case TYPE_UNKNOWN:
            default:
                log("handleCpbrCommand - invalid chars");
                atCommandErrorCode = BluetoothCmeError.TEXT_HAS_INVALID_CHARS;
                mNativeInterface.atResponseCode(remoteDevice, atCommandResult, atCommandErrorCode);
        }
    }

    /** Get the most recent result for the given phone book,
     *  with the cursor ready to go.
     *  If force then re-query that phonebook
     *  Returns null if the cursor is not ready
     */
    private synchronized PhonebookResult getPhonebookResult(String pb, boolean force) {
        if (pb == null) {
            return null;
        }
        PhonebookResult pbr = mPhonebooks.get(pb);
        if (pbr == null) {
            pbr = new PhonebookResult();
        }
        if (force || pbr.cursor == null) {
            if (!queryPhonebook(pb, pbr)) {
                return null;
            }
        }

        return pbr;
    }

    private synchronized boolean queryPhonebook(String pb, PhonebookResult pbr) {
        String where;
        boolean ancillaryPhonebook = true;
        boolean simPhonebook = false;

        if (pb.equals("ME")) {
            ancillaryPhonebook = false;
            where = null;
        } else if (pb.equals("DC")) {
            where = OUTGOING_CALL_WHERE;
        } else if (pb.equals("RC")) {
            where = INCOMING_CALL_WHERE;
        } else if (pb.equals("MC")) {
            where = MISSED_CALL_WHERE;
        } else if (pb.equals("SM")) {
            ancillaryPhonebook = false;
            simPhonebook = true;
            where = VISIBLE_SIM_PHONEBOOK_WHERE;
        } else {
            return false;
        }

        if (pbr.cursor != null) {
            pbr.cursor.close();
            pbr.cursor = null;
        }

        if (ancillaryPhonebook) {
            try {
                Bundle queryArgs = new Bundle();
                queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, where);
                queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SORT_ORDER,
                                       Calls.DEFAULT_SORT_ORDER);
                queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, MAX_PHONEBOOK_SIZE);
                pbr.cursor = mContentResolver.query(Calls.CONTENT_URI, CALLS_PROJECTION,
                                   queryArgs, null);
                if (pbr.cursor == null) {
                    return false;
                }
            } catch (Exception e) {
                Log.e(TAG, "Exception while querying the call log database", e);
                return false;
            }

            pbr.numberColumn = pbr.cursor.getColumnIndexOrThrow(Calls.NUMBER);
            pbr.numberPresentationColumn =
                    pbr.cursor.getColumnIndexOrThrow(Calls.NUMBER_PRESENTATION);
            pbr.typeColumn = -1;
            pbr.nameColumn = -1;
        } else {
            Log.i(TAG, "simPhonebook " + simPhonebook);
            if (simPhonebook) {
                final Uri mysimUri = Uri.parse(SIM_URI);
                try {
                    Bundle queryArgs = new Bundle();
                    queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, where);
                    queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, MAX_PHONEBOOK_SIZE);
                    pbr.cursor = mContentResolver.query(mysimUri, SIM_PROJECTION,
                                   queryArgs, null);
                    Log.i(TAG, "querySIMcontactbook where " + where + " uri :" + mysimUri);
                    if (pbr.cursor == null) {
                        Log.i(TAG, "querying phone contacts on sim returned null.");
                        return false;
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Exception while querying sim contact book database", e);
                    return false;
                }

                pbr.numberColumn = NUMBER_COLUMN_INDEX;
                pbr.numberPresentationColumn = -1;
                pbr.typeColumn = NUMBERTYPE_COLUMN_INDEX;
                pbr.nameColumn = NAME_COLUMN_INDEX;
                Log.i(TAG, " pbr.numberColumn: " + pbr.numberColumn +
                           " pbr.numberPresentationColumn: " + pbr.numberPresentationColumn +
                           " pbr.typeColumn: " + pbr.typeColumn +
                           " pbr.nameColumn: " + pbr.nameColumn);
            } else {
                final Uri phoneContentUri = DevicePolicyUtils.getEnterprisePhoneUri(mContext);
                try {
                    Bundle queryArgs = new Bundle();
                    queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, where);
                    queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, MAX_PHONEBOOK_SIZE);
                    pbr.cursor = mContentResolver.query(phoneContentUri, PHONES_PROJECTION,
                                   queryArgs, null);
                    Log.i(TAG, "queryPhonebook where " + where + " uri :" + phoneContentUri);
                    if (pbr.cursor == null) {
                        Log.i(TAG, "querying phone contacts on memory returned null.");
                        return false;
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Exception while querying phone contacts on memory", e);
                    return false;
                }

                Log.i(TAG, "Phone.NUMBER: " + Phone.NUMBER + " Phone.TYPE :" + Phone.TYPE +
                              "Phone.DISPLAY_NAME :" + Phone.DISPLAY_NAME);
                pbr.numberColumn = pbr.cursor.getColumnIndex(Phone.NUMBER);
                pbr.numberPresentationColumn = -1;
                pbr.typeColumn = pbr.cursor.getColumnIndex(Phone.TYPE);
                pbr.nameColumn = pbr.cursor.getColumnIndex(Phone.DISPLAY_NAME);
                Log.i(TAG, " pbr.numberColumn: " + pbr.numberColumn +
                           " pbr.numberPresentationColumn: " + pbr.numberPresentationColumn +
                           " pbr.typeColumn: " + pbr.typeColumn +
                           " pbr.nameColumn: " + pbr.nameColumn);
            }
        }
        Log.i(TAG, "Refreshed phonebook " + pb + " with " + pbr.cursor.getCount() + " results");
        return true;
    }

    synchronized void resetAtState() {
        mCharacterSet = "UTF-8";
        mCpbrIndex1 = mCpbrIndex2 = -1;
        mCheckingAccessPermission = false;
    }

    private synchronized int getMaxPhoneBookSize(int currSize) {
        // some car kits ignore the current size and request max phone book
        // size entries. Thus, it takes a long time to transfer all the
        // entries. Use a heuristic to calculate the max phone book size
        // considering future expansion.
        // maxSize = currSize + currSize / 2 rounded up to nearest power of 2
        // If currSize < 100, use 100 as the currSize

        int maxSize = (currSize < 100) ? 100 : currSize;
        maxSize += maxSize / 2;
        return roundUpToPowerOfTwo(maxSize);
    }

    private int roundUpToPowerOfTwo(int x) {
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    // process CPBR command after permission check
    /*package*/ int processCpbrCommand(BluetoothDevice device) {
        log("processCpbrCommand");
        int atCommandResult = HeadsetHalConstants.AT_RESPONSE_ERROR;
        int atCommandErrorCode = -1;
        String atCommandResponse = null;
        StringBuilder response = new StringBuilder();
        String record;

        // Check phonebook
        PhonebookResult pbr = getPhonebookResult(mCurrentPhonebook, true); //false);
        if (pbr == null) {
            Log.e(TAG, "pbr is null");
            atCommandErrorCode = BluetoothCmeError.OPERATION_NOT_ALLOWED;
            return atCommandResult;
        }

        // More sanity checks
        // Send OK instead of ERROR if these checks fail.
        // When we send error, certain kits like BMW disconnect the
        // Handsfree connection.
        if (pbr.cursor.getCount() == 0 || mCpbrIndex1 <= 0 || mCpbrIndex2 < mCpbrIndex1
                || mCpbrIndex1 > pbr.cursor.getCount()) {
            atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
            Log.e(TAG, "Invalid request or no results, returning");
            return atCommandResult;
        }

        if (mCpbrIndex2 > pbr.cursor.getCount()) {
            Log.w(TAG, "max index requested is greater than number of records"
                    + " available, resetting it");
            mCpbrIndex2 = pbr.cursor.getCount();
        }
        // Process
        atCommandResult = HeadsetHalConstants.AT_RESPONSE_OK;
        int errorDetected = -1; // no error
        pbr.cursor.moveToPosition(mCpbrIndex1 - 1);
        log("mCpbrIndex1 = " + mCpbrIndex1 + " and mCpbrIndex2 = " + mCpbrIndex2);
        for (int index = mCpbrIndex1; index <= mCpbrIndex2; index++) {
            String number = pbr.cursor.getString(pbr.numberColumn);
            String name = null;
            int type = -1;
            if (pbr.nameColumn == -1 && number != null && number.length() > 0) {
                // try caller id lookup
                // TODO: This code is horribly inefficient. I saw it
                // take 7 seconds to process 100 missed calls.
                try {
                    Cursor c = mContentResolver.query(
                            Uri.withAppendedPath(PhoneLookup.ENTERPRISE_CONTENT_FILTER_URI,
                            number), new String[]{
                                PhoneLookup.DISPLAY_NAME, PhoneLookup.TYPE
                            }, null, null, null);
                    if (c != null) {
                        if (c.moveToFirst()) {
                            name = c.getString(0);
                            type = c.getInt(1);
                        }
                        c.close();
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Exception while querying phonebook database", e);
                    return HeadsetHalConstants.AT_RESPONSE_ERROR;
                }

                if (DBG && name == null) {
                    log("Caller ID lookup failed for " + number);
                }

            } else if (pbr.nameColumn != -1) {
                name = pbr.cursor.getString(pbr.nameColumn);
            } else {
                log("processCpbrCommand: empty name and number");
            }
            if (name == null) {
                name = "";
            }
            name = name.trim();
            if (name.length() > 28) {
                name = name.substring(0, 28);
            }

            if (pbr.typeColumn != -1) {
                type = pbr.cursor.getInt(pbr.typeColumn);
                name = name + "/" + getPhoneType(type);
            }

            if (number == null) {
                number = "";
            }
            int regionType = PhoneNumberUtils.toaFromString(number);

            number = number.trim();
            number = PhoneNumberUtils.stripSeparators(number);
            if (number.length() > 30) {
                number = number.substring(0, 30);
            }
            int numberPresentation = Calls.PRESENTATION_ALLOWED;
            if (pbr.numberPresentationColumn != -1) {
                numberPresentation = pbr.cursor.getInt(pbr.numberPresentationColumn);
            }
            if (numberPresentation != Calls.PRESENTATION_ALLOWED) {
                number = "";
                // TODO: there are 3 types of numbers should have resource
                // strings for: unknown, private, and payphone
                name = mContext.getString(R.string.unknownNumber);
            }

            // TODO(): Handle IRA commands. It's basically
            // a 7 bit ASCII character set.
            if (!name.isEmpty() && mCharacterSet.equals("GSM")) {
                byte[] nameByte = GsmAlphabet.stringToGsm8BitPacked(name);
                if (nameByte == null) {
                    name = mContext.getString(R.string.unknownNumber);
                } else {
                    name = new String(nameByte);
                }
            }

            record = "+CPBR: " + index + ",\"" + number + "\"," + regionType + ",\"" + name + "\"";
            record = record + "\r\n\r\n";
            atCommandResponse = record;
            mNativeInterface.atResponseString(device, atCommandResponse);
            if (!pbr.cursor.moveToNext()) {
                break;
            }
        }
        if (pbr.cursor != null) {
            pbr.cursor.close();
            pbr.cursor = null;
        }
        return atCommandResult;
    }

    /**
     * Checks if the remote device has premission to read our phone book.
     * If the return value is {@link BluetoothDevice#ACCESS_UNKNOWN}, it means this method has sent
     * an Intent to Settings application to ask user preference.
     *
     * @return {@link BluetoothDevice#ACCESS_UNKNOWN}, {@link BluetoothDevice#ACCESS_ALLOWED} or
     *         {@link BluetoothDevice#ACCESS_REJECTED}.
     */
    private int checkAccessPermission(BluetoothDevice remoteDevice) {
        log("checkAccessPermission");
        int permission = remoteDevice.getPhonebookAccessPermission();

        if (permission == BluetoothDevice.ACCESS_UNKNOWN) {
            log("checkAccessPermission - ACTION_CONNECTION_ACCESS_REQUEST");
            Intent intent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_REQUEST);
            intent.setPackage(mPairingPackage);
            intent.putExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                    BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, remoteDevice);
            // Leave EXTRA_PACKAGE_NAME and EXTRA_CLASS_NAME field empty.
            // BluetoothHandsfree's broadcast receiver is anonymous, cannot be targeted.
            mContext.sendOrderedBroadcast(intent, BLUETOOTH_CONNECT,
                    Utils.getTempAllowlistBroadcastOptions(), null, null,
                    Activity.RESULT_OK, null, null);
        }

        return permission;
    }

    private static String getPhoneType(int type) {
        switch (type) {
            case Phone.TYPE_HOME:
                return "H";
            case Phone.TYPE_MOBILE:
                return "M";
            case Phone.TYPE_WORK:
                return "W";
            case Phone.TYPE_FAX_HOME:
            case Phone.TYPE_FAX_WORK:
                return "F";
            case Phone.TYPE_OTHER:
            case Phone.TYPE_CUSTOM:
            default:
                return "O";
        }
    }

    private static void log(String msg) {
        Log.d(TAG, msg);
    }
}
