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

package com.android.bluetooth.btservice;

import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothCodecConfig;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.media.AudioManager;
import android.provider.Settings;
import android.util.FeatureFlagUtils;
import android.util.Log;
import android.os.SystemProperties;

import com.android.bluetooth.R;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.avrcp.AvrcpTargetService;
import com.android.bluetooth.avrcpcontroller.AvrcpControllerService;
import com.android.bluetooth.gatt.GattService;
import com.android.bluetooth.hearingaid.HearingAidService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hfpclient.HeadsetClientService;
import com.android.bluetooth.hid.HidDeviceService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.map.BluetoothMapService;
import com.android.bluetooth.mapclient.MapClientService;
import com.android.bluetooth.opp.BluetoothOppService;
import com.android.bluetooth.pan.PanService;
import com.android.bluetooth.pbap.BluetoothPbapService;
import com.android.bluetooth.pbapclient.PbapClientService;
import com.android.bluetooth.ReflectionUtils;
import com.android.bluetooth.sap.SapService;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.ba.BATService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
public class Config {
    private static final String TAG = "AdapterServiceConfig";
    private static final boolean DBG = true;
    protected static int adv_audio_feature_mask;
    public static final int ADV_AUDIO_UNICAST_FEAT_MASK = 0x01;
    public static final int ADV_AUDIO_BCA_FEAT_MASK = 0x02;
    public static final int ADV_AUDIO_BCS_FEAT_MASK = 0x04;

    private static Class mBCServiceClass = null;
    private static Class mBroadcastClass = null;
    private static Class mPCServiceClass = null;
    private static Class mCcServiceClass = null;
    private static Class mGroupServiceClass = null;
    private static ArrayList<Class> profiles = new ArrayList<>();
    private static boolean mIsA2dpSink, mIsSplitSink, mIsBAEnabled, mIsSplitA2dpEnabled;
    private static boolean mIsHfpClient;
    private static boolean mIsPbapClient;

    static {
        mBCServiceClass = ReflectionUtils.getRequiredClass(
                "com.android.bluetooth.bc.BCService");
        mBroadcastClass = ReflectionUtils.getRequiredClass(
                "com.android.bluetooth.broadcast.BroadcastService");
        mPCServiceClass = ReflectionUtils.getRequiredClass(
                "com.android.bluetooth.pc.PCService");
        mCcServiceClass = ReflectionUtils.getRequiredClass(
                "com.android.bluetooth.cc.CCService");
        mGroupServiceClass = ReflectionUtils.getRequiredClass(
                "com.android.bluetooth.groupclient.GroupService");
    }

    private static class ProfileConfig {
        Class mClass;
        int mSupported;
        long mMask;

        ProfileConfig(Class theClass, int supportedFlag, long mask) {
            mClass = theClass;
            mSupported = supportedFlag;
            mMask = mask;
        }
    }

    /**
     * List of profile services with the profile-supported resource flag and bit mask.
     */
    private static final ProfileConfig[] PROFILE_SERVICES_AND_FLAGS = {
            new ProfileConfig(HeadsetService.class, R.bool.profile_supported_hs_hfp,
                    (1 << BluetoothProfile.HEADSET)),
            new ProfileConfig(A2dpService.class, R.bool.profile_supported_a2dp,
                    (1 << BluetoothProfile.A2DP)),
            new ProfileConfig(A2dpSinkService.class, R.bool.profile_supported_a2dp_sink,
                    (1 << BluetoothProfile.A2DP_SINK)),
            new ProfileConfig(HidHostService.class, R.bool.profile_supported_hid_host,
                    (1 << BluetoothProfile.HID_HOST)),
            new ProfileConfig(PanService.class, R.bool.profile_supported_pan,
                    (1 << BluetoothProfile.PAN)),
            new ProfileConfig(GattService.class, R.bool.profile_supported_gatt,
                    (1 << BluetoothProfile.GATT)),
            new ProfileConfig(BluetoothMapService.class, R.bool.profile_supported_map,
                    (1 << BluetoothProfile.MAP)),
            new ProfileConfig(HeadsetClientService.class, R.bool.profile_supported_hfpclient,
                    (1 << BluetoothProfile.HEADSET_CLIENT)),
            new ProfileConfig(AvrcpTargetService.class, R.bool.profile_supported_avrcp_target,
                    (1 << BluetoothProfile.AVRCP)),
            new ProfileConfig(AvrcpControllerService.class,
                    R.bool.profile_supported_avrcp_controller,
                    (1 << BluetoothProfile.AVRCP_CONTROLLER)),
            new ProfileConfig(SapService.class, R.bool.profile_supported_sap,
                    (1 << BluetoothProfile.SAP)),
            new ProfileConfig(PbapClientService.class, R.bool.profile_supported_pbapclient,
                    (1 << BluetoothProfile.PBAP_CLIENT)),
            new ProfileConfig(MapClientService.class, R.bool.profile_supported_mapmce,
                    (1 << BluetoothProfile.MAP_CLIENT)),
            new ProfileConfig(HidDeviceService.class, R.bool.profile_supported_hid_device,
                    (1 << BluetoothProfile.HID_DEVICE)),
            new ProfileConfig(BluetoothOppService.class, R.bool.profile_supported_opp,
                    (1 << BluetoothProfile.OPP)),
            new ProfileConfig(BluetoothPbapService.class, R.bool.profile_supported_pbap,
                    (1 << BluetoothProfile.PBAP)),
            new ProfileConfig(HearingAidService.class,
                    com.android.internal.R.bool.config_hearing_aid_profile_supported,
                    (1 << BluetoothProfile.HEARING_AID)),
            new ProfileConfig(BATService.class, R.bool.profile_supported_ba,
                    (1 << BATService.BA_TRANSMITTER)),
    };

    /* List of Unicast Advance Audio Profiles */
    private static ArrayList<ProfileConfig> unicastAdvAudioProfiles =
            new ArrayList<ProfileConfig>(
                Arrays.asList(
                    new ProfileConfig(ApmConstIntf.MusicPlayerControlService,
                        R.bool.profile_supported_music_player_service,
                        (1 << BluetoothProfile.MCP_SERVER)),
                    new ProfileConfig(mCcServiceClass, R.bool.profile_supported_cc_server,
                        (1 << BluetoothProfile.CC_SERVER))
            ));

    /* List of Broadcast Advance Audio Profiles */
    private static ArrayList<ProfileConfig> broadcastAdvAudioProfiles =
            new ArrayList<ProfileConfig>(
                Arrays.asList(
                    new ProfileConfig(mBCServiceClass, R.bool.profile_supported_bac,
                        (1 << BluetoothProfile.BC_PROFILE)),
                    new ProfileConfig(mBroadcastClass,
                         R.bool.profile_supported_broadcast,
                        (1 << BluetoothProfile.BROADCAST)),
                    new ProfileConfig(mPCServiceClass, R.bool.profile_supported_pc,
                        (1 << BluetoothProfile.PC_PROFILE))
            ));

    /* List of Profiles common for Unicast and Broadcast advance audio features */
    private static ArrayList<ProfileConfig> commonAdvAudioProfiles =
            new ArrayList<ProfileConfig>(
                Arrays.asList(
                    new ProfileConfig(mGroupServiceClass,
                            R.bool.profile_supported_group_client,
                            (1 << BluetoothProfile.GROUP_CLIENT)),
                    new ProfileConfig(ApmConstIntf.CoordinatedAudioService,
                            R.bool.profile_supported_ca,
                            (1 << ApmConstIntf.COORDINATED_AUDIO_UNICAST)),
                    new ProfileConfig(ApmConstIntf.StreamAudioService,
                            R.bool.profile_supported_le_audio,
                            (1 << ApmConstIntf.LE_AUDIO_UNICAST))
            ));

    private static Class[] sSupportedProfiles = new Class[0];

    static void init(Context ctx) {
        if (ctx == null) {
            return;
        }
        Resources resources = ctx.getResources();
        if (resources == null) {
            return;
        }

        if (isAdvAudioAvailable()) {
            AdapterService.setAdvanceAudioSupport();
            initAdvAudioConfig(ctx);
        }

        profiles.clear();
        getAudioProperties();
        for (ProfileConfig config : PROFILE_SERVICES_AND_FLAGS) {
            boolean supported = resources.getBoolean(config.mSupported);

            if (!supported && (config.mClass == HearingAidService.class) && FeatureFlagUtils
                                .isEnabled(ctx, FeatureFlagUtils.HEARING_AID_SETTINGS)) {
                Log.v(TAG, "Feature Flag enables support for HearingAidService");
                supported = true;
            }

            if (supported && !isProfileDisabled(ctx, config.mMask)) {
                if (!addAudioProfiles(config.mClass.getSimpleName())) {
                    Log.i(TAG, " Profile " + config.mClass.getSimpleName() + " Not added ");
                    continue;
                }
                // ignore adding map server service for targets where map client is enabled
                if ((config.mClass.getSimpleName().equals("BluetoothMapService")) &&
                    (mIsSplitSink)) {
                    Log.i(TAG, " Profile " + config.mClass.getSimpleName() + " Not added ");
                    continue;
                }
                // ignore adding map client service for targets where map client is disabled
                if ((config.mClass.getSimpleName().equals("MapClientService")) &&
                    (!mIsSplitSink)) {
                    Log.i(TAG, " Profile " + config.mClass.getSimpleName() + " Not added ");
                    continue;
                }
                // ignore adding pbap client service for targets where pbap client is disabled
                if ((config.mClass.getSimpleName().equals("PbapClientService")) &&
                    (!mIsPbapClient)) {
                    Log.i(TAG, " Profile " + config.mClass.getSimpleName() + " Not added ");
                    continue;
                }
                Log.v(TAG, "Adding " + config.mClass.getSimpleName());
                profiles.add(config.mClass);
            }
        }
        sSupportedProfiles = profiles.toArray(new Class[profiles.size()]);
    }

    static void initAdvAudioSupport(Context ctx) {
        if (ctx == null || !isAdvAudioAvailable()) {
            Log.w(TAG, "Context is null or advance audio features are unavailable");
            return;
        }

        Resources resources = ctx.getResources();
        if (resources == null) {
            return;
        }

        adv_audio_feature_mask = SystemProperties.getInt(
                                    "persist.vendor.service.bt.adv_audio_mask", 0);
        boolean isCCEnabled = SystemProperties.getBoolean(
                                    "persist.vendor.service.bt.cc", true);
        setAdvAudioMaskFromHostAddOnBits();

        Log.d(TAG, "adv_audio_feature_mask = " + adv_audio_feature_mask);
        ArrayList<Class> advAudioProfiles = new ArrayList<>();

        /* Add common advance audio profiles */
        if (((adv_audio_feature_mask & ADV_AUDIO_UNICAST_FEAT_MASK) != 0) ||
            ((adv_audio_feature_mask & ADV_AUDIO_BCA_FEAT_MASK ) != 0) ||
            ((adv_audio_feature_mask & ADV_AUDIO_BCS_FEAT_MASK ) != 0)) {
            for (ProfileConfig config : commonAdvAudioProfiles) {
                if (config.mClass == null) continue;
                boolean supported = resources.getBoolean(config.mSupported);
                if (supported) {
                    if ((config.mClass == mGroupServiceClass ||
                            config.mClass == ApmConstIntf.CoordinatedAudioService) &&
                        (((adv_audio_feature_mask & ADV_AUDIO_UNICAST_FEAT_MASK) == 0) &&
                         ((adv_audio_feature_mask & ADV_AUDIO_BCA_FEAT_MASK) == 0))) {
                        continue;
                    }
                    Log.d(TAG, "Adding " + config.mClass.getSimpleName());
                    advAudioProfiles.add(config.mClass);
                }
            }
        }

        /* Add unicast advance audio profiles */
        if ((adv_audio_feature_mask & ADV_AUDIO_UNICAST_FEAT_MASK) != 0) {
            for (ProfileConfig config : unicastAdvAudioProfiles) {
                if (config.mClass == null) continue;
                boolean supported = resources.getBoolean(config.mSupported);
                if (config.mClass.getSimpleName().equals("CCService") &&
                    isCCEnabled == false) {
                    Log.d(TAG," isCCEnabled = " + isCCEnabled);
                    continue;
                } else if (supported && config.mClass != null) {
                    Log.d(TAG, "Adding " + config.mClass.getSimpleName());
                    advAudioProfiles.add(config.mClass);
                }
            }
        }

        /* Add broadcast advance audio profiles */
        if ((adv_audio_feature_mask & ADV_AUDIO_BCA_FEAT_MASK) != 0 ||
            (adv_audio_feature_mask & ADV_AUDIO_BCS_FEAT_MASK) != 0) {
            for (ProfileConfig config : broadcastAdvAudioProfiles) {
                if (config.mClass == null) continue;
                boolean supported = resources.getBoolean(config.mSupported);
                if (supported && config.mClass != null) {
                    if ((config.mClass == mBroadcastClass) &&
                           (adv_audio_feature_mask & ADV_AUDIO_BCS_FEAT_MASK) == 0) {
                        continue;
                    } else if ((config.mClass == mBCServiceClass ||
                                config.mClass == mPCServiceClass) &&
                            ((adv_audio_feature_mask & ADV_AUDIO_BCA_FEAT_MASK) == 0)) {
                        continue;
                    }
                    Log.d(TAG, "Adding " + config.mClass.getSimpleName());
                    advAudioProfiles.add(config.mClass);
                }
            }
        }

        // Copy advance audio profiles to sSupportedProfiles
        List<Class> allProfiles = new ArrayList<Class>();
        allProfiles.addAll(profiles);
        allProfiles.addAll(advAudioProfiles);
        sSupportedProfiles = allProfiles.toArray(new Class[allProfiles.size()]);
    }

    protected static void initAdvAudioConfig (Context ctx) {
        adv_audio_feature_mask = SystemProperties.getInt(
                                    "persist.vendor.service.bt.adv_audio_mask", 0);
        Log.d(TAG, "initAdvAudioConfig: adv_audio_feature_mask = " + adv_audio_feature_mask);
        if (!isLC3CodecSupported(ctx)) {
            adv_audio_feature_mask &= ~ADV_AUDIO_UNICAST_FEAT_MASK;
            adv_audio_feature_mask &= ~ADV_AUDIO_BCS_FEAT_MASK;
            SystemProperties.set("persist.vendor.service.bt.adv_audio_mask",
                    String.valueOf(adv_audio_feature_mask));
            SystemProperties.set("persist.vendor.btstack.lc3_reset_adv_audio_mask",
                    "true");
            Log.w(TAG, "LC3 Codec is not supported. adv_audio_feature_mask = "
                    + adv_audio_feature_mask);
        } else {
            /* Recovery mechanism to re-enable LC3 based features */
            boolean isLC3Reenabled = SystemProperties.getBoolean(
                   "persist.vendor.btstack.lc3_reset_adv_audio_mask", false);
            if (isLC3Reenabled) {
                adv_audio_feature_mask |= ADV_AUDIO_UNICAST_FEAT_MASK;
                adv_audio_feature_mask |= ADV_AUDIO_BCS_FEAT_MASK;
                SystemProperties.set("persist.vendor.service.bt.adv_audio_mask",
                        String.valueOf(adv_audio_feature_mask));
                SystemProperties.set("persist.vendor.btstack.lc3_reset_adv_audio_mask",
                        "false");
                Log.i(TAG, "LC3 is reenabled. Updated adv_audio_feature_mask = "
                        + adv_audio_feature_mask);
            }
        }
    }

    protected static boolean isLC3CodecSupported(Context ctx) {
      boolean isLC3Supported = false;
      AudioManager mAudioManager = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
      List<BluetoothCodecConfig> mmSupportedCodecs =
              mAudioManager.getHwOffloadEncodingFormatsSupportedForA2DP();

      for (BluetoothCodecConfig codecConfig: mmSupportedCodecs) {
          if (codecConfig.getCodecType() == BluetoothCodecConfig.SOURCE_CODEC_TYPE_LC3) {
              isLC3Supported = true;
              Log.d(TAG, " LC3 codec supported : " + codecConfig);
              break;
          }
      }

      return isLC3Supported;
    }

    /* API to set Advance Audio mask based in Host AddOn Feature bits */
    private static void setAdvAudioMaskFromHostAddOnBits() {
        AdapterService service = AdapterService.getAdapterService();
        if (service == null){
            Log.e(TAG, "Adapter Service is NULL.");
            return;
        }

        if (!service.isHostAdvAudioUnicastFeatureSupported() &&
                !service.isHostAdvAudioStereoRecordingFeatureSupported()) {
            Log.i(TAG, "Stereo recording and unicast features are disabled"
                    + " in Host AddOn features");
            adv_audio_feature_mask &= ~ADV_AUDIO_UNICAST_FEAT_MASK;
        }

        if (!service.isHostAdvAudioBCAFeatureSupported()) {
            Log.i(TAG, "BCA disabled in Host AddOn features");
            adv_audio_feature_mask &= ~ADV_AUDIO_BCA_FEAT_MASK;
        }

        if (!service.isHostAdvAudioBCSFeatureSupported()) {
            Log.i(TAG, "BCS disabled in Host AddOn features");
            adv_audio_feature_mask &= ~ADV_AUDIO_BCS_FEAT_MASK;
        }

        SystemProperties.set("persist.vendor.service.bt.adv_audio_mask",
                String.valueOf(adv_audio_feature_mask));
    }

    /* Returns true if advance audio project is available */
    public static boolean isAdvAudioAvailable() {
        return (mGroupServiceClass != null ? true : false);
    }

    static Class[] getSupportedProfiles() {
        return sSupportedProfiles;
    }

    private static long getProfileMask(Class profile) {
        for (ProfileConfig config : PROFILE_SERVICES_AND_FLAGS) {
            if (config.mClass == profile) {
                return config.mMask;
            }
        }
        for (ProfileConfig config : commonAdvAudioProfiles) {
            if (config.mClass == profile) {
                return config.mMask;
            }
        }
        for (ProfileConfig config : unicastAdvAudioProfiles) {
            if (config.mClass == profile) {
                return config.mMask;
            }
        }
        for (ProfileConfig config : broadcastAdvAudioProfiles) {
            if (config.mClass == profile) {
                return config.mMask;
            }
        }
        Log.w(TAG, "Could not find profile bit mask for " + profile.getSimpleName());
        return 0;
    }

    static long getSupportedProfilesBitMask() {
        long mask = 0;
        for (final Class profileClass : getSupportedProfiles()) {
            mask |= getProfileMask(profileClass);
        }
        return mask;
    }

    private static boolean isProfileDisabled(Context context, long profileMask) {
        final ContentResolver resolver = context.getContentResolver();
        final long disabledProfilesBitMask =
                Settings.Global.getLong(resolver, Settings.Global.BLUETOOTH_DISABLED_PROFILES, 0);

        return (disabledProfilesBitMask & profileMask) != 0;
    }

    private static synchronized boolean addAudioProfiles(String serviceName) {
        /* If property not enabled and request is for A2DPSinkService, don't add */
        if ((serviceName.equals("A2dpSinkService")) && (!mIsA2dpSink))
            return false;
        if ((serviceName.equals("A2dpService")) && (mIsA2dpSink))
            return false;
        if (serviceName.equals("BATService")) {
            return mIsBAEnabled && mIsSplitA2dpEnabled;
        }
        if ((serviceName.equals("HeadsetClientService")) && (!mIsHfpClient))
            return false;
        if ((serviceName.equals("HeadsetService")) && (mIsHfpClient))
            return false;

        // always return true for other profiles
        return true;
    }

    private static void getAudioProperties() {
        mIsSplitSink = SystemProperties.getBoolean("persist.vendor.bluetooth.split_a2dp_sink", false);

        if (mIsSplitSink) {
           SystemProperties.set("persist.vendor.service.bt.a2dp.sink", "true");
           SystemProperties.set("persist.vendor.bt.a2dp.sink_conn", "2");
        }

        mIsA2dpSink = SystemProperties.getBoolean("persist.vendor.service.bt.a2dp.sink", false);
        mIsBAEnabled = SystemProperties.getBoolean("persist.vendor.service.bt.bca", false);
        mIsHfpClient = SystemProperties.getBoolean("persist.vendor.bluetooth.hfp_client", false);
        mIsPbapClient = SystemProperties.getBoolean("persist.vendor.bluetooth.pbap_client", false);
        // Split A2dp will be enabled by default
        mIsSplitA2dpEnabled = true;
        AdapterService adapterService = AdapterService.getAdapterService();
        if (adapterService != null) {
            mIsSplitA2dpEnabled = adapterService.isSplitA2dpEnabled();
        } else {
            Log.e(TAG,"adapterService is null");
        }
        if (DBG) {
            Log.d(TAG, "getAudioProperties mIsA2dpSink " + mIsA2dpSink + " mIsBAEnabled "
                + mIsBAEnabled + " mIsSplitA2dpEnabled " + mIsSplitA2dpEnabled);
        }
        Log.d(TAG, "getAudioProperties mIsHfpClient" + mIsHfpClient + " mIsPbapClient " + mIsPbapClient);
    }
}
