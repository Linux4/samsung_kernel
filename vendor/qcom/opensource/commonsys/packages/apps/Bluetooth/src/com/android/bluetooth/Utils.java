/*
 Not a contribution.
*/

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

package com.android.bluetooth;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.BLUETOOTH_ADVERTISE;
import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.Manifest.permission.BLUETOOTH_SCAN;
import static android.Manifest.permission.RENOUNCE_PERMISSIONS;
import static android.content.PermissionChecker.PERMISSION_HARD_DENIED;
import static android.content.PermissionChecker.PID_UNKNOWN;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.os.PowerExemptionManager.TEMPORARY_ALLOW_LIST_TYPE_FOREGROUND_SERVICE_ALLOWED;

import android.Manifest;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.annotation.SuppressLint;
import android.app.AppGlobals;
import android.app.AppOpsManager;
import android.app.BroadcastOptions;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.companion.AssociationInfo;
import android.companion.CompanionDeviceManager;
import android.content.AttributionSource;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.PermissionChecker;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.location.LocationManager;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelUuid;
import android.os.PowerExemptionManager;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.DeviceConfig;
import android.util.Log;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.time.Instant;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

/**
 * @hide
 */

public final class Utils {
    private static final String TAG = "BluetoothUtils";
    private static final int MICROS_PER_UNIT = 625;
    private static final String PTS_TEST_MODE_PROPERTY = "persist.bluetooth.pts";
    private static final String KEY_TEMP_ALLOW_LIST_DURATION_MS = "temp_allow_list_duration_ms";
    private static final long DEFAULT_TEMP_ALLOW_LIST_DURATION_MS = 20_000;

    static final int BD_ADDR_LEN = 6; // bytes
    static final int BD_UUID_LEN = 16; // bytes

    public static @Nullable String getName(@Nullable BluetoothDevice device) {
        final AdapterService service = AdapterService.getAdapterService();
        if (service != null && device != null) {
            return service.getRemoteName(device);
        } else {
            return null;
        }
    }
    public static String getAddressStringFromByte(byte[] address) {
        if (address == null || address.length != BD_ADDR_LEN) {
            return null;
        }

        return String.format("%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2],
                address[3], address[4], address[5]);
    }

    public static byte[] getByteAddress(BluetoothDevice device) {
        if (device == null) return new byte[BD_ADDR_LEN];
        return getBytesFromAddress(device.getAddress());
   }

    public static byte[] addressToBytes(String address) {
        return getBytesFromAddress(address);
    }

    public static byte[] getBytesFromAddress(String address) {
        int i, j = 0;
        byte[] output = new byte[BD_ADDR_LEN];

        for (i = 0; i < address.length(); i++) {
            if (address.charAt(i) != ':') {
                output[j] = (byte) Integer.parseInt(address.substring(i, i + 2), BD_UUID_LEN);
                j++;
                i++;
            }
        }

        return output;
    }

    public static int byteArrayToInt(byte[] valueBuf) {
        return byteArrayToInt(valueBuf, 0);
    }

    public static short byteArrayToShort(byte[] valueBuf) {
        ByteBuffer converter = ByteBuffer.wrap(valueBuf);
        converter.order(ByteOrder.nativeOrder());
        return converter.getShort();
    }

    public static int byteArrayToInt(byte[] valueBuf, int offset) {
        ByteBuffer converter = ByteBuffer.wrap(valueBuf);
        converter.order(ByteOrder.nativeOrder());
        return converter.getInt(offset);
    }

    public static String byteArrayToString(byte[] valueBuf) {
        StringBuilder sb = new StringBuilder();
        for (int idx = 0; idx < valueBuf.length; idx++) {
            if (idx != 0) {
                sb.append(" ");
            }
            sb.append(String.format("%02x", valueBuf[idx]));
        }
        return sb.toString();
    }

    /**
     * A parser to transfer a byte array to a UTF8 string
     *
     * @param valueBuf the byte array to transfer
     * @return the transferred UTF8 string
     */
    public static String byteArrayToUtf8String(byte[] valueBuf) {
        CharsetDecoder decoder = Charset.forName("UTF8").newDecoder();
        ByteBuffer byteBuffer = ByteBuffer.wrap(valueBuf);
        String valueStr = "";
        try {
            valueStr = decoder.decode(byteBuffer).toString();
        } catch (Exception ex) {
            Log.e(TAG, "Error when parsing byte array to UTF8 String. " + ex);
        }
        return valueStr;
    }

    public static byte[] intToByteArray(int value) {
        ByteBuffer converter = ByteBuffer.allocate(4);
        converter.order(ByteOrder.nativeOrder());
        converter.putInt(value);
        return converter.array();
    }

    public static byte[] uuidToByteArray(ParcelUuid pUuid) {
        int length = BD_UUID_LEN;
        ByteBuffer converter = ByteBuffer.allocate(length);
        converter.order(ByteOrder.BIG_ENDIAN);
        long msb, lsb;
        UUID uuid = pUuid.getUuid();
        msb = uuid.getMostSignificantBits();
        lsb = uuid.getLeastSignificantBits();
        converter.putLong(msb);
        converter.putLong(8, lsb);
        return converter.array();
    }

    public static byte[] uuidsToByteArray(ParcelUuid[] uuids) {
        int length = uuids.length * BD_UUID_LEN;
        ByteBuffer converter = ByteBuffer.allocate(length);
        converter.order(ByteOrder.BIG_ENDIAN);
        UUID uuid;
        long msb, lsb;
        for (int i = 0; i < uuids.length; i++) {
            uuid = uuids[i].getUuid();
            msb = uuid.getMostSignificantBits();
            lsb = uuid.getLeastSignificantBits();
            converter.putLong(i * BD_UUID_LEN, msb);
            converter.putLong(i * BD_UUID_LEN + 8, lsb);
        }
        return converter.array();
    }

    public static ParcelUuid[] byteArrayToUuid(byte[] val) {
        int numUuids = val.length / BD_UUID_LEN;
        ParcelUuid[] puuids = new ParcelUuid[numUuids];
        UUID uuid;
        int offset = 0;

        ByteBuffer converter = ByteBuffer.wrap(val);
        converter.order(ByteOrder.BIG_ENDIAN);

        for (int i = 0; i < numUuids; i++) {
            puuids[i] = new ParcelUuid(
                    new UUID(converter.getLong(offset), converter.getLong(offset + 8)));
            offset += BD_UUID_LEN;
        }
        return puuids;
    }

    public static String debugGetAdapterStateString(int state) {
        switch (state) {
            case BluetoothAdapter.STATE_OFF:
                return "STATE_OFF";
            case BluetoothAdapter.STATE_ON:
                return "STATE_ON";
            case BluetoothAdapter.STATE_TURNING_ON:
                return "STATE_TURNING_ON";
            case BluetoothAdapter.STATE_TURNING_OFF:
                return "STATE_TURNING_OFF";
            default:
                return "UNKNOWN";
        }
    }

    public static String ellipsize(String s) {
        // Only ellipsize release builds
        if (!Build.TYPE.equals("user")) {
            return s;
        }
        if (s == null) {
            return null;
        }
        if (s.length() < 3) {
            return s;
        }
        return s.charAt(0) + "⋯" + s.charAt(s.length() - 1);
    }

    public static void copyStream(InputStream is, OutputStream os, int bufferSize)
            throws IOException {
        if (is != null && os != null) {
            byte[] buffer = new byte[bufferSize];
            int bytesRead = 0;
            while ((bytesRead = is.read(buffer)) >= 0) {
                os.write(buffer, 0, bytesRead);
            }
        }
    }

    public static void safeCloseStream(InputStream is) {
        if (is != null) {
            try {
                is.close();
            } catch (Throwable t) {
                Log.d(TAG, "Error closing stream", t);
            }
        }
    }

    public static void safeCloseStream(OutputStream os) {
        if (os != null) {
            try {
                os.close();
            } catch (Throwable t) {
                Log.d(TAG, "Error closing stream", t);
            }
        }
    }

    static int sSystemUiUid = UserHandle.USER_NULL;
    public static void setSystemUiUid(int uid) {
        Utils.sSystemUiUid = uid;
    }

    static int sForegroundUserId = UserHandle.USER_NULL;
    public static void setForegroundUserId(int uid) {
        Utils.sForegroundUserId = uid;
    }

    /**
     * Enforces that a Companion Device Manager (CDM) association exists between the calling
     * application and the Bluetooth Device.
     *
     * @param cdm the CompanionDeviceManager object
     * @param context the Bluetooth AdapterService context
     * @param callingPackage the calling package
     * @param callingUid the calling app uid
     * @param device the remote BluetoothDevice
     * @return {@code true} if there is a CDM association
     * @throws SecurityException if the package name does not match the uid or the association
     *                           doesn't exist
     */
    public static boolean enforceCdmAssociation(CompanionDeviceManager cdm, Context context,
            String callingPackage, int callingUid, BluetoothDevice device) {
        if (!isPackageNameAccurate(context, callingPackage, callingUid)) {
            throw new SecurityException("hasCdmAssociation: Package name " + callingPackage
                    + " is inaccurate for calling uid " + callingUid);
        }

        for (AssociationInfo association : cdm.getAllAssociations()) {
            if (association.getPackageName().equals(callingPackage)
                    && !association.isSelfManaged() && device.getAddress() != null
                    && association.getDeviceMacAddress() != null
                    && device.getAddress().equalsIgnoreCase(
                            association.getDeviceMacAddress().toString())) {
                return true;
            }
        }
        throw new SecurityException("The application with package name " + callingPackage
                + " does not have a CDM association with the Bluetooth Device");
    }

    /**
     * Verifies whether the calling package name matches the calling app uid
     * @param context the Bluetooth AdapterService context
     * @param callingPackage the calling application package name
     * @param callingUid the calling application uid
     * @return {@code true} if the package name matches the calling app uid, {@code false} otherwise
     */
    public static boolean isPackageNameAccurate(Context context, String callingPackage,
            int callingUid) {
        UserHandle callingUser = UserHandle.getUserHandleForUid(callingUid);

        // Verifies the integrity of the calling package name
        try {
            int packageUid = context.createContextAsUser(callingUser, 0)
                    .getPackageManager().getPackageUid(callingPackage, 0);
            if (packageUid != callingUid) {
                Log.e(TAG, "isPackageNameAccurate: App with package name " + callingPackage
                        + " is UID " + packageUid + " but caller is " + callingUid);
                return false;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "isPackageNameAccurate: App with package name " + callingPackage
                    + " does not exist");
            return false;
        }
        return true;
    }

    /**
     * Checks whether the caller has the BLUETOOTH_PRIVILEGED permission
     *
     * @param context the Bluetooth AdapterService context
     * @return {@code true} if the caller has the BLUETOOTH_PRIVILEGED permission, {@code false}
     *         otherwise
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean hasBluetoothPrivilegedPermission(Context context) {
        return context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_PRIVILEGED)
                == PackageManager.PERMISSION_GRANTED;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public static void enforceBluetoothPrivilegedPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH PRIVILEGED permission");
    }

    @RequiresPermission(android.Manifest.permission.LOCAL_MAC_ADDRESS)
    public static void enforceLocalMacAddressPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.LOCAL_MAC_ADDRESS,
                "Need LOCAL_MAC_ADDRESS permission");
    }

    @RequiresPermission(android.Manifest.permission.DUMP)
    public static void enforceDumpPermission(Context context) {
        context.enforceCallingOrSelfPermission(
                android.Manifest.permission.DUMP,
                "Need DUMP permission");
    }

    public static AttributionSource getCallingAttributionSource() {
        int callingUid = Binder.getCallingUid();
        if (callingUid == android.os.Process.ROOT_UID) {
            callingUid = android.os.Process.SYSTEM_UID;
        }
        try {
            return new AttributionSource(callingUid,
                    AppGlobals.getPackageManager().getPackagesForUid(callingUid)[0], null);
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to resolve AttributionSource", e);
        }
    }

    public static AttributionSource getCallingAttributionSource(Context context) {
        int callingUid = Binder.getCallingUid();
        if (callingUid == android.os.Process.ROOT_UID) {
            callingUid = android.os.Process.SYSTEM_UID;
        }
        return new AttributionSource.Builder(callingUid)
            .setPackageName(context.getPackageManager().getPackagesForUid(callingUid)[0])
            .build();
    }

    @SuppressLint("AndroidFrameworkRequiresPermission")
    private static boolean checkPermissionForPreflight(Context context, String permission) {
        final int result = PermissionChecker.checkCallingOrSelfPermissionForPreflight(
                context, permission);
        if (result == PERMISSION_GRANTED) {
            return true;
        }

        final String msg = "Need " + permission + " permission";
        if (result == PERMISSION_HARD_DENIED) {
            throw new SecurityException(msg);
        } else {
            Log.w(TAG, msg);
            return false;
        }
    }

    @SuppressLint("AndroidFrameworkRequiresPermission")
    private static boolean checkPermissionForDataDelivery(Context context, String permission,
            AttributionSource attributionSource, String message) {
        final int result = PermissionChecker.checkPermissionForDataDeliveryFromDataSource(
                context, permission, PID_UNKNOWN,
                new AttributionSource(context.getAttributionSource(), attributionSource), message);
        if (result == PERMISSION_GRANTED) {
            return true;
        }

        final String msg = "Need " + permission + " permission for " + attributionSource + ": "
                + message;
        if (result == PERMISSION_HARD_DENIED) {
            throw new SecurityException(msg);
        } else {
            Log.w(TAG, msg);
            return false;
        }
    }

    /**
     * Returns true if the BLUETOOTH_CONNECT permission is granted for the calling app. Returns
     * false if the result is a soft denial. Throws SecurityException if the result is a hard
     * denial.
     *
     * <p>Should be used in situations where the app op should not be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public static boolean checkConnectPermissionForPreflight(Context context) {
        return checkPermissionForPreflight(context, BLUETOOTH_CONNECT);
    }

    /**
     * Returns true if the BLUETOOTH_CONNECT permission is granted for the calling app. Returns
     * false if the result is a soft denial. Throws SecurityException if the result is a hard
     * denial.
     *
     * <p>Should be used in situations where data will be delivered and hence the app op should
     * be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public static boolean checkConnectPermissionForDataDelivery(
            Context context, AttributionSource attributionSource, String message) {
        return checkPermissionForDataDelivery(context, BLUETOOTH_CONNECT,
                attributionSource, message);
    }

    /**
     * Returns true if the BLUETOOTH_SCAN permission is granted for the calling app. Returns false
     * if the result is a soft denial. Throws SecurityException if the result is a hard denial.
     *
     * <p>Should be used in situations where the app op should not be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    public static boolean checkScanPermissionForPreflight(Context context) {
        return checkPermissionForPreflight(context, BLUETOOTH_SCAN);
    }

    /**
     * Returns true if the BLUETOOTH_SCAN permission is granted for the calling app. Returns false
     * if the result is a soft denial. Throws SecurityException if the result is a hard denial.
     *
     * <p>Should be used in situations where data will be delivered and hence the app op should
     * be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    public static boolean checkScanPermissionForDataDelivery(
            Context context, AttributionSource attributionSource, String message) {
        return checkPermissionForDataDelivery(context, BLUETOOTH_SCAN,
                attributionSource, message);
    }

    /**
     * Returns true if the BLUETOOTH_ADVERTISE permission is granted for the
     * calling app. Returns false if the result is a soft denial. Throws
     * SecurityException if the result is a hard denial.
     * <p>
     * Should be used in situations where the app op should not be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    public static boolean checkAdvertisePermissionForPreflight(Context context) {
        return checkPermissionForPreflight(context, BLUETOOTH_ADVERTISE);
    }

    /**
     * Returns true if the BLUETOOTH_ADVERTISE permission is granted for the
     * calling app. Returns false if the result is a soft denial. Throws
     * SecurityException if the result is a hard denial.
     * <p>
     * Should be used in situations where data will be delivered and hence the
     * app op should be noted.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    public static boolean checkAdvertisePermissionForDataDelivery(
            Context context, AttributionSource attributionSource, String message) {
        return checkPermissionForDataDelivery(context, BLUETOOTH_ADVERTISE,
                attributionSource, message);
    }

    /**
     * Returns true if the specified package has disavowed the use of bluetooth scans for location,
     * that is, if they have specified the {@code neverForLocation} flag on the BLUETOOTH_SCAN
     * permission.
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean hasDisavowedLocationForScan(
            Context context, AttributionSource attributionSource, boolean inTestMode) {

        // Check every step along the attribution chain for a renouncement.
        // If location has been renounced anywhere in the chain we treat it as a disavowal.
        AttributionSource currentAttrib = attributionSource;
        while (true) {
            if (currentAttrib.getRenouncedPermissions().contains(ACCESS_FINE_LOCATION)
                    && (inTestMode || context.checkPermission(RENOUNCE_PERMISSIONS, -1,
                    currentAttrib.getUid())
                    == PackageManager.PERMISSION_GRANTED)) {
                return true;
            }
            AttributionSource nextAttrib = currentAttrib.getNext();
            if (nextAttrib == null) {
                break;
            }
            currentAttrib = nextAttrib;
        }

        // Check the last attribution in the chain for a neverForLocation disavowal.
        String packageName = currentAttrib.getPackageName();
        PackageManager pm = context.getPackageManager();
        try {
            // TODO(b/183478032): Cache PackageInfo for use here.
            PackageInfo pkgInfo = pm.getPackageInfo(packageName, GET_PERMISSIONS);
            for (int i = 0; i < pkgInfo.requestedPermissions.length; i++) {
                if (pkgInfo.requestedPermissions[i].equals(BLUETOOTH_SCAN)) {
                    return (pkgInfo.requestedPermissionsFlags[i]
                            & PackageInfo.REQUESTED_PERMISSION_NEVER_FOR_LOCATION) != 0;
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Could not find package for disavowal check: " + packageName);
        }
        return false;
    }

    public static boolean checkCallerIsSystemOrActiveUser() {
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();
        return (sForegroundUserId == callingUser)
                || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
    }

    public static boolean checkCallerIsSystemOrActiveUser(String tag) {
        final boolean res = checkCallerIsSystemOrActiveUser();
        if (!res) {
            Log.w(TAG, tag + " - Not allowed for non-active user and non-system user");
        }
        return res;
    }

    public static boolean callerIsSystemOrActiveUser(String tag, String method) {
        return checkCallerIsSystemOrActiveUser(tag + "." + method + "()");
    }

    public static boolean checkCallerIsSystemOrActiveOrManagedUser(Context context) {
        if (context == null) {
            return checkCallerIsSystemOrActiveUser();
        }
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();

        // Use the Bluetooth process identity when making call to get parent user
        long ident = Binder.clearCallingIdentity();
        try {
            UserManager um = (UserManager) context.getSystemService(Context.USER_SERVICE);
            UserInfo ui = um.getProfileParent(callingUser);
            int parentUser = (ui != null) ? ui.id : UserHandle.USER_NULL;

            // Always allow SystemUI/System access.
            return (sForegroundUserId == callingUser) || (sForegroundUserId == parentUser)
                    || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                    || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
        } catch (Exception ex) {
            Log.e(TAG, "checkCallerIsSystemOrActiveOrManagedUser: Exception ex=" + ex);
            return false;
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public static boolean checkCallerIsSystemOrActiveOrManagedUser(Context context, String tag) {
        final boolean res = checkCallerIsSystemOrActiveOrManagedUser(context);
        if (!res) {
            Log.w(TAG, tag + " - Not allowed for"
                    + " non-active user and non-system and non-managed user");
        }
        return res;
    }

    public static boolean callerIsSystemOrActiveOrManagedUser(Context context, String tag,
            String method) {
        return checkCallerIsSystemOrActiveOrManagedUser(context, tag + "." + method + "()");
    }

    public static boolean checkServiceAvailable(ProfileService service, String tag) {
        if (service == null) {
            Log.w(TAG, tag + " - Not present");
            return false;
        }
        if (!service.isAvailable()) {
            Log.w(TAG, tag + " - Not available");
            return false;
        }
        return true;
    }

    /**
     * Checks whether location is off and must be on for us to perform some operation
     */
    public static boolean blockedByLocationOff(Context context, UserHandle userHandle) {
        return !context.getSystemService(LocationManager.class)
                .isLocationEnabledForUser(userHandle);
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_COARSE_LOCATION and
     * OP_COARSE_LOCATION is allowed
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasCoarseLocation(
            Context context, AttributionSource attributionSource, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        if (PermissionChecker.checkPermissionForDataDeliveryFromDataSource(
                context, ACCESS_COARSE_LOCATION, PID_UNKNOWN,
                new AttributionSource(context.getAttributionSource(), attributionSource),
                "Bluetooth location check") == PERMISSION_GRANTED) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_COARSE_LOCATION "
                + "permission to get scan results");
        return false;
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_COARSE_LOCATION and
     * OP_COARSE_LOCATION is allowed or android.Manifest.permission.ACCESS_FINE_LOCATION and
     * OP_FINE_LOCATION is allowed
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasCoarseOrFineLocation(
            Context context, AttributionSource attributionSource, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        if (PermissionChecker.checkPermissionForDataDeliveryFromDataSource(
                context, ACCESS_FINE_LOCATION, PID_UNKNOWN,
                new AttributionSource(context.getAttributionSource(), attributionSource),
                "Bluetooth location check") == PERMISSION_GRANTED) {
            return true;
        }

        if (PermissionChecker.checkPermissionForDataDeliveryFromDataSource(
                context, ACCESS_COARSE_LOCATION, PID_UNKNOWN,
                new AttributionSource(context.getAttributionSource(), attributionSource),
                "Bluetooth location check") == PERMISSION_GRANTED) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_COARSE_LOCATION or ACCESS_FINE_LOCATION"
                + "permission to get scan results");
        return false;
    }

    /**
     * Checks that calling process has android.Manifest.permission.ACCESS_FINE_LOCATION and
     * OP_FINE_LOCATION is allowed
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasFineLocation(
            Context context, AttributionSource attributionSource, UserHandle userHandle) {
        if (blockedByLocationOff(context, userHandle)) {
            Log.e(TAG, "Permission denial: Location is off.");
            return false;
        }

        if (PermissionChecker.checkPermissionForDataDeliveryFromDataSource(
                context, ACCESS_FINE_LOCATION, PID_UNKNOWN,
                new AttributionSource(context.getAttributionSource(), attributionSource),
                "Bluetooth location check") == PERMISSION_GRANTED) {
            return true;
        }

        Log.e(TAG, "Permission denial: Need ACCESS_FINE_LOCATION "
                + "permission to get scan results");
        return false;
    }

    /**
     * Returns true if the caller holds NETWORK_SETTINGS
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasNetworkSettingsPermission(Context context) {
        return context.checkCallingOrSelfPermission(android.Manifest.permission.NETWORK_SETTINGS)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if the caller holds NETWORK_SETUP_WIZARD
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasNetworkSetupWizardPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.NETWORK_SETUP_WIZARD)
                        == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Returns true if the caller holds RADIO_SCAN_WITHOUT_LOCATION
     */
    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasScanWithoutLocationPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.RADIO_SCAN_WITHOUT_LOCATION)
                == PackageManager.PERMISSION_GRANTED;
    }

    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasPrivilegedPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH_PRIVILEGED)
                == PackageManager.PERMISSION_GRANTED;
    }

    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    public static boolean checkCallerHasWriteSmsPermission(Context context) {
        return context.checkCallingOrSelfPermission(
                android.Manifest.permission.WRITE_SMS) == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Checks that the target sdk of the app corresponding to the provided package name is greater
     * than or equal to the passed in target sdk.
     * <p>
     * For example, if the calling app has target SDK {@link Build.VERSION_CODES#S} and we pass in
     * the targetSdk {@link Build.VERSION_CODES#R}, the API will return true because S >= R.
     *
     * @param context Bluetooth service context
     * @param pkgName caller's package name
     * @param expectedMinimumTargetSdk one of the values from {@link Build.VERSION_CODES}
     * @return {@code true} if the caller's target sdk is greater than or equal to
     * expectedMinimumTargetSdk, {@code false} otherwise
     */
    public static boolean checkCallerTargetSdk(Context context, String pkgName,
            int expectedMinimumTargetSdk) {
        try {
            return context.getPackageManager().getApplicationInfo(pkgName, 0).targetSdkVersion
                    >= expectedMinimumTargetSdk;
        } catch (PackageManager.NameNotFoundException e) {
            // In case of exception, assume true
        }
        return true;
    }

    private static boolean isAppOppAllowed(AppOpsManager appOps, String op, String callingPackage,
            @NonNull String callingFeatureId) {
        return appOps.noteOp(op, Binder.getCallingUid(), callingPackage, callingFeatureId, null)
                == AppOpsManager.MODE_ALLOWED;
    }

    /**
     * Converts {@code millisecond} to unit. Each unit is 0.625 millisecond.
     */
    public static int millsToUnit(int milliseconds) {
        return (int) (TimeUnit.MILLISECONDS.toMicros(milliseconds) / MICROS_PER_UNIT);
    }

    /**
     * Check if we are running in BluetoothInstrumentationTest context by trying to load
     * com.android.bluetooth.FileSystemWriteTest. If we are not in Instrumentation test mode, this
     * class should not be found. Thus, the assumption is that FileSystemWriteTest must exist.
     * If FileSystemWriteTest is removed in the future, another test class in
     * BluetoothInstrumentationTest should be used instead
     *
     * @return true if in BluetoothInstrumentationTest, false otherwise
     */
    public static boolean isInstrumentationTestMode() {
        try {
            return Class.forName("com.android.bluetooth.FileSystemWriteTest") != null;
        } catch (ClassNotFoundException exception) {
            return false;
        }
    }

    /**
     * Throws {@link IllegalStateException} if we are not in BluetoothInstrumentationTest. Useful
     * for ensuring certain methods only get called in BluetoothInstrumentationTest
     */
    public static void enforceInstrumentationTestMode() {
        if (!isInstrumentationTestMode()) {
            throw new IllegalStateException("Not in BluetoothInstrumentationTest");
        }
    }

    /**
     * Check if we are running in PTS test mode. To enable/disable PTS test mode, invoke
     * {@code adb shell setprop persist.bluetooth.pts true/false}
     *
     * @return true if in PTS Test mode, false otherwise
     */
    public static boolean isPtsTestMode() {
        return SystemProperties.getBoolean(PTS_TEST_MODE_PROPERTY, false);
    }

    /**
     * Get uid/pid string in a binder call
     *
     * @return "uid/pid=xxxx/yyyy"
     */
    public static String getUidPidString() {
        return "uid/pid=" + Binder.getCallingUid() + "/" + Binder.getCallingPid();
    }

    /**
     * Get system local time
     *
     * @return "MM-dd HH:mm:ss.SSS"
     */
    public static String getLocalTimeString() {
        return DateTimeFormatter.ofPattern("MM-dd HH:mm:ss.SSS")
                .withZone(ZoneId.systemDefault()).format(Instant.now());
    }

    public static void skipCurrentTag(XmlPullParser parser)
            throws XmlPullParserException, IOException {
        int outerDepth = parser.getDepth();
        int type;
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                && (type != XmlPullParser.END_TAG
                || parser.getDepth() > outerDepth)) {
        }
    }


    public static @NonNull Bundle getTempAllowlistBroadcastOptions() {
        // Use the Bluetooth process identity to pass permission check when reading DeviceConfig
        final long ident = Binder.clearCallingIdentity();
        final BroadcastOptions bOptions = BroadcastOptions.makeBasic();
        try {
            final long durationMs = DeviceConfig.getLong(DeviceConfig.NAMESPACE_BLUETOOTH,
                    KEY_TEMP_ALLOW_LIST_DURATION_MS, DEFAULT_TEMP_ALLOW_LIST_DURATION_MS);
            bOptions.setTemporaryAppAllowlist(durationMs,
                    TEMPORARY_ALLOW_LIST_TYPE_FOREGROUND_SERVICE_ALLOWED,
                    PowerExemptionManager.REASON_BLUETOOTH_BROADCAST, "");
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
        return bOptions.toBundle();
    }

    public static boolean checkCaller() {
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();
        return (sForegroundUserId == callingUser)
                || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
    }

    public static boolean checkCallerAllowManagedProfiles(Context mContext) {
        if (mContext == null) {
            return checkCaller();
        }
        int callingUser = UserHandle.getCallingUserId();
        int callingUid = Binder.getCallingUid();

        // Use the Bluetooth process identity when making call to get parent user
        long ident = Binder.clearCallingIdentity();
        try {
            UserManager um = (UserManager) mContext.getSystemService(Context.USER_SERVICE);
            UserInfo ui = um.getProfileParent(callingUser);
            int parentUser = (ui != null) ? ui.id : UserHandle.USER_NULL;

            // Always allow SystemUI/System access.
            return (sForegroundUserId == callingUser) || (sForegroundUserId == parentUser)
                    || (UserHandle.getAppId(sSystemUiUid) == UserHandle.getAppId(callingUid))
                    || (UserHandle.getAppId(Process.SYSTEM_UID) == UserHandle.getAppId(callingUid));
        } catch (Exception ex) {
            Log.e(TAG, "checkCallerAllowManagedProfiles: Exception ex=" + ex);
            return false;
        } finally {
            Binder.restoreCallingIdentity(ident);
        }
    }

    public static void enforceBluetoothPermission(Context context) {
        context.enforceCallingOrSelfPermission(
            android.Manifest.permission.BLUETOOTH, "Need BLUETOOTH permission");
    }

    public static void enforceBluetoothAdminPermission(Context context) {
        context.enforceCallingOrSelfPermission(
            android.Manifest.permission.BLUETOOTH_ADMIN, "Need BLUETOOTH ADMIN permission");
    }

    /**
     * Checks that value is present as at least one of the elements of the array.
     * @param array the array to check in
     * @param value the value to check for
     * @return true if the value is present in the array
     */
    public static <T> boolean arrayContains(@Nullable T[] array, T value) {
        if (array == null) return false;
        for (T element : array) {
            if (Objects.equals(element, value)) return true;
        }
        return false;
    }
}
