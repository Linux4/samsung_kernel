/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package com.qualcomm.qti.server.qtiwifi;

import android.annotation.NonNull;
import vendor.qti.hardware.wifi.supplicant.ISupplicantVendor;
import vendor.qti.hardware.wifi.supplicant.ISupplicantVendorStaIface;
import vendor.qti.hardware.wifi.supplicant.IVendorIfaceInfo;
import vendor.qti.hardware.wifi.supplicant.IVendorIfaceType;
import vendor.qti.hardware.wifi.supplicant.SupplicantVendorStatusCode;
import android.os.IBinder;
import android.os.IBinder.DeathRecipient;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.ServiceSpecificException;
import android.util.Log;

import com.qualcomm.qti.server.qtiwifi.util.GeneralUtil.Mutable;
import java.util.HashMap;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * HAL calls to set up/tear down the supplicant daemon and make requests
 * related to station mode. Uses the Vendor AIDL supplicant interface.
 */
public class QtiSupplicantStaIfaceHalAidlImpl implements IQtiSupplicantStaIfaceHal {
    private static final String TAG = "QtiSupplicantStaIfaceHalAidlImpl";
    private static final String HAL_INSTANCE_NAME = ISupplicantVendor.DESCRIPTOR + "/default";

    private final Object mLock = new Object();
    private String mVendorIfaceName = null;

    // Supplicant AIDL interface objects
    private ISupplicantVendor mISupplicantVendor = null;
    private Map<String, ISupplicantVendorStaIface> mISupplicantVendorStaIfaces = new HashMap<>();
    private SupplicantDeathRecipient mSupplicantVendorDeathRecipient;
    private class SupplicantDeathRecipient implements DeathRecipient {
        @Override
        public void binderDied() {
                synchronized (mLock) {
                    Log.w(TAG, "ISupplicant binder died.");
                    supplicantVendorServiceDiedHandler();
                }
        }
    }

    public QtiSupplicantStaIfaceHalAidlImpl() {
        mSupplicantVendorDeathRecipient = new SupplicantDeathRecipient();
        Log.i(TAG, "QtiSupplicantStaIfaceHalAidlImpl() invoked");
    }

    /**
     * Checks whether the ISupplicantVendor service is declared, and therefore should be available.
     *
     * @return true if the ISupplicantVendor service is declared
     */
    public boolean initialize() {
        synchronized (mLock) {
            if (mISupplicantVendor != null) {
                Log.i(TAG, "Service is already initialized, skipping initialize method");
                return true;
            }
            Log.i(TAG, "Checking for ISupplicant service.");
            mISupplicantVendorStaIfaces.clear();
            getSupplicantVendorInstance();
            return serviceDeclared();
        }
    }

    /**
     * Wrapper functions to access HAL objects, created to be mockable in unit tests
     */
    protected ISupplicantVendor getSupplicantVendorMockable() {
        synchronized (mLock) {
            try {
                return ISupplicantVendor.Stub.asInterface(
                        ServiceManager.waitForDeclaredService(HAL_INSTANCE_NAME));
            } catch (Exception e) {
                Log.e(TAG, "Unable to get ISupplicantVendor service, " + e);
                return null;
            }
        }
    }

    protected IBinder getServiceBinderMockable() {
        synchronized (mLock) {
            if (mISupplicantVendor == null) {
                return null;
            }
            return mISupplicantVendor.asBinder();
        }
    }

    private boolean getSupplicantVendorInstance() {

        final String methodStr = "getSupplicantVendorInstance";
        if (mISupplicantVendor != null) {
            Log.i(TAG, "Service is already initialized, skipping " + methodStr);
            return true;
        }

        mISupplicantVendor = getSupplicantVendorMockable();
        if (mISupplicantVendor == null) {
            Log.e(TAG, "Unable to obtain ISupplicant binder.");
            return false;
        }
        Log.i(TAG, "Obtained ISupplicantVendor binder.");

        try {
            IBinder serviceBinder = getServiceBinderMockable();
            if (serviceBinder == null) {
                return false;
            }
            serviceBinder.linkToDeath(mSupplicantVendorDeathRecipient, /* flags= */  0);
            return true;
        } catch (RemoteException e) {
            handleRemoteException(e, methodStr);
            return false;
        }
    }
    /**
     * Indicates whether the AIDL service is declared
     */
    public static boolean serviceDeclared() {
        return ServiceManager.isDeclared(HAL_INSTANCE_NAME);
    }

    /**
     * Helper method to look up the specified iface.
     */
    private ISupplicantVendorStaIface fetchVendorStaIface() {
        synchronized (mLock) {
            /** List all vendor supplicant STA Ifaces */
            IVendorIfaceInfo[] supplicantVendorStaIfaces = new IVendorIfaceInfo[10];
            try {
                final String methodStr = "listVendorInterfaces";
                if (!checkSupplicantVendorAndLogFailure(methodStr)) return null;
                supplicantVendorStaIfaces = mISupplicantVendor.listVendorInterfaces();
            } catch (RemoteException e) {
                Log.e(TAG, "ISupplicantVendor.listInterfaces exception: " + e);
                supplicantVendorServiceDiedHandler();
                return null;
            }
            if (supplicantVendorStaIfaces.length == 0) {
                Log.e(TAG, "Got zero HIDL supplicant vendor Sta ifaces. Stopping supplicant vendor HIDL startup.");
                return null;
            }
            Mutable<ISupplicantVendorStaIface> supplicantVendorStaIface = new Mutable<>();
            for (IVendorIfaceInfo ifaceInfo : supplicantVendorStaIfaces) {
                if (ifaceInfo.type == IVendorIfaceType.STA) {
                    try {
                        final String methodStr = "getVendorInterface";
                        if (!checkSupplicantVendorAndLogFailure(methodStr)) return null;
                        mVendorIfaceName = ifaceInfo.name;
                        supplicantVendorStaIface.value = mISupplicantVendor.getVendorInterface(ifaceInfo);
                    } catch (RemoteException e) {
                        Log.e(TAG, "ISupplicantVendor.getInterface exception: " + e);
                        supplicantVendorServiceDiedHandler();
                        return null;
                    }
                    break;
                }
            }
            return supplicantVendorStaIface.value;
        }
    }

    /**
     * Returns false if SupplicantVendor is null, and logs failure to call methodStr
     */
    private boolean checkSupplicantVendorAndLogFailure(final String methodStr) {
        synchronized (mLock) {
            if (mISupplicantVendor == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendor is null");
                return false;
            }
            return true;
        }
    }

    private void handleRemoteException(RemoteException e, String methodStr) {
        synchronized (mLock) {
            Log.e(TAG, "ISupplicantVendorStaIface." + methodStr + " failed with exception", e);
        }
    }

    private void handleServiceSpecificException(ServiceSpecificException e, String methodStr) {
        synchronized (mLock) {
            Log.e(TAG, "ISupplicantVendorStaIface." + methodStr + " failed with exception", e);
        }
    }

    /**
     * Handle supplicantvendor death.
     */
    private void supplicantVendorServiceDiedHandler() {
        synchronized (mLock) {
            mISupplicantVendor = null;
            mISupplicantVendorStaIfaces.clear();
        }
    }

    /**
     * Setup a STA interface for the specified iface name.
     *
     * @param ifaceName Name of the interface.
     * @return true on success, false otherwise.
     */
    public boolean setupVendorIface(@NonNull String ifaceName) {
        synchronized (mLock) {
            final String methodStr = "setupVendorIface";
            ISupplicantVendorStaIface vendor_iface = null;
            vendor_iface = fetchVendorStaIface();
            if (vendor_iface == null) {
                Log.e(TAG, "Failed to get vendor iface");
                return false;
            }
            try {
                IBinder serviceBinder = getServiceBinderMockable();
                if (serviceBinder == null) {
                    return false;
                }
                serviceBinder.linkToDeath(mSupplicantVendorDeathRecipient, /* flags= */  0);
            } catch (RemoteException e) {
                handleRemoteException(e, methodStr);
                return false;
            }
            mISupplicantVendorStaIfaces.put(mVendorIfaceName, vendor_iface);
                return true;
            }
    }

    /**
     * Helper method to look up the vendor_iface object for the specified iface.
     */
    private ISupplicantVendorStaIface getVendorStaIface(@NonNull String ifaceName) {
        return mISupplicantVendorStaIfaces.get(ifaceName);
    }

     /**
     * run Driver command
     *
     * @param command Driver Command
     * @return status
     */
    public String doDriverCmd(String command)
    {
        synchronized (mLock) {
            final String methodStr = "doDriverCmd";
            final Mutable<String> reply = new Mutable<>();

            reply.value = "";
            ISupplicantVendorStaIface vendorStaIface = getVendorStaIface(mVendorIfaceName);
            if (vendorStaIface == null) {
                Log.e(TAG, "Can't call " + methodStr + ", ISupplicantVendorStaIface is null");
                return null;
            }

            try {
                reply.value = vendorStaIface.doDriverCmd(command);
            } catch (RemoteException e) {
                Log.e(TAG, "doDriverCmd failed with RemoteException");
                handleRemoteException(e, methodStr);
            } catch (ServiceSpecificException e) {
                Log.e(TAG, "doDriverCmd failed with ServiceSpecificException");
                handleServiceSpecificException(e, methodStr);
            }
            return reply.value;
         }
    }
}

