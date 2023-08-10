/*
 * Copyright (c) 2008-2009, Motorola, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * - Neither the name of the Motorola, Inc. nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth.opp;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.UnsupportedEncodingException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;

/**
 * This class stores information about a single receiving file. It will only be
 * used for inbounds share, e.g. receive a file to determine a correct save file
 * name
 */
public class BluetoothOppReceiveFileInfo {
    private static final boolean D = Constants.DEBUG;
    private static final boolean V = Constants.VERBOSE;
    private static String sDesiredStoragePath = null;

    /* To truncate the name of the received file if the length exceeds 237 */
    private static final int OPP_LENGTH_OF_FILE_NAME = 237;


    /** absolute store file name */
    public String mFileName;

    public long mLength;

    public int mStatus;

    public String mData;

    public Uri mInsertUri;

    public BluetoothOppReceiveFileInfo(String data, long length, int status) {
        mData = data;
        mStatus = status;
        mLength = length;
    }

    public BluetoothOppReceiveFileInfo(String filename, long length, Uri insertUri, int status) {
        mFileName = filename;
        mStatus = status;
        mInsertUri = insertUri;
        mLength = length;
    }

    public BluetoothOppReceiveFileInfo(int status) {
        this(null, 0, null, status);
    }

    // public static final int BATCH_STATUS_CANCELED = 4;
    public static BluetoothOppReceiveFileInfo generateFileInfo(Context context, int id) {

        ContentResolver contentResolver = context.getContentResolver();
        Uri contentUri = Uri.parse(BluetoothShare.CONTENT_URI + "/" + id);
        String filename = null, hint = null, mimeType = null;
        long length = 0;
        Cursor metadataCursor = contentResolver.query(contentUri, new String[]{
                BluetoothShare.FILENAME_HINT, BluetoothShare.TOTAL_BYTES, BluetoothShare.MIMETYPE
        }, null, null, null);
        if (metadataCursor != null) {
            try {
                if (metadataCursor.moveToFirst()) {
                    hint = metadataCursor.getString(0);
                    length = metadataCursor.getLong(1);
                    mimeType = metadataCursor.getString(2);
                }
            } finally {
                metadataCursor.close();
            }
        }

        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            if (D) {
                Log.d(Constants.TAG, "Receive File aborted - no external storage");
            }
            return new BluetoothOppReceiveFileInfo(BluetoothShare.STATUS_ERROR_NO_SDCARD);
        }

        filename = choosefilename(hint);
        if (filename == null) {
            // should not happen. It must be pre-rejected
            return new BluetoothOppReceiveFileInfo(BluetoothShare.STATUS_FILE_ERROR);
        }
        String extension = null;
        int dotIndex = filename.lastIndexOf(".");
        if (dotIndex < 0) {
            if (mimeType == null) {
                // should not happen. It must be pre-rejected
                return new BluetoothOppReceiveFileInfo(BluetoothShare.STATUS_FILE_ERROR);
            } else {
                extension = "";
            }
        } else {
            extension = filename.substring(dotIndex);
            filename = filename.substring(0, dotIndex);
        }
        if (D) {
            Log.d(Constants.TAG, " File Name " + filename);
        }

        if (filename.getBytes().length > OPP_LENGTH_OF_FILE_NAME) {
          /* Including extn of the file, Linux supports 255 character as a maximum length of the
           * file name to be created. Hence, Instead of sending OBEX_HTTP_INTERNAL_ERROR,
           * as a response, truncate the length of the file name and save it. This check majorly
           * helps in the case of vcard, where Phone book app supports contact name to be saved
           * more than 255 characters, But the server rejects the card just because the length of
           * vcf file name received exceeds 255 Characters.
           */
            Log.i(Constants.TAG, " File Name Length :" + filename.length());
            Log.i(Constants.TAG, " File Name Length in Bytes:" + filename.getBytes().length);

            try {
                byte[] oldfilename = filename.getBytes("UTF-8");
                byte[] newfilename = new byte[OPP_LENGTH_OF_FILE_NAME];
                System.arraycopy(oldfilename, 0, newfilename, 0, OPP_LENGTH_OF_FILE_NAME);
                filename = new String(newfilename, "UTF-8");
            } catch (UnsupportedEncodingException e) {
                Log.e(Constants.TAG, "Exception: " + e);
            }
            if (D) {
                Log.d(Constants.TAG, "File name is too long. Name is truncated as: " + filename);
            }
        }

        DateFormat dateFormat = new SimpleDateFormat("_hhmmss");
        String currentTime = dateFormat.format(Calendar.getInstance().getTime());
        String fullfilename = filename + currentTime + extension;

        if (V) {
            Log.v(Constants.TAG, "Generated received filename " + fullfilename);
        }

        Uri insertUri = null;
        ContentValues mediaContentValues = new ContentValues();
        mediaContentValues.put(MediaStore.MediaColumns.DISPLAY_NAME, fullfilename);
        mediaContentValues.put(MediaStore.MediaColumns.MIME_TYPE, mimeType);
        mediaContentValues.put(MediaStore.MediaColumns.RELATIVE_PATH,
                Environment.DIRECTORY_DOWNLOADS);
        insertUri = contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,
                mediaContentValues);

        if (insertUri == null) {
            if (D) {
                Log.e(Constants.TAG, "Error when creating file " + fullfilename);
            }
            return new BluetoothOppReceiveFileInfo(BluetoothShare.STATUS_FILE_ERROR);
        }

        Log.d(Constants.TAG, "file crated, insertUri:" + insertUri.toString());

        return new BluetoothOppReceiveFileInfo(fullfilename, length, insertUri, 0);
    }

    private static String choosefilename(String hint) {
        String filename = null;

        // First, try to use the hint from the application, if there's one
        if (filename == null && !(hint == null) && !hint.endsWith("/") && !hint.endsWith("\\")) {
            // Prevent abuse of path backslashes by converting all backlashes '\\' chars
            // to UNIX-style forward-slashes '/'
            hint = hint.replace('\\', '/');
            // Convert all whitespace characters to spaces.
            hint = hint.replaceAll("\\s", " ");
            // Replace illegal fat filesystem characters from the
            // filename hint i.e. :"<>*?| with something safe.
            hint = hint.replaceAll("[:\"<>*?|]", "_");
            if (V) {
                Log.v(Constants.TAG, "getting filename from hint");
            }
            int index = hint.lastIndexOf('/') + 1;
            if (index > 0) {
                filename = hint.substring(index);
            } else {
                filename = hint;
            }
        }
        return filename;
    }
}
