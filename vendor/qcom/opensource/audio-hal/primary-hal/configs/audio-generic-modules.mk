ifeq ($(TARGET_USES_QMAA),true)
    ifneq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO),true)
	        #QMAA Mode is enabled
        TARGET_IS_HEADLESS := true
    endif
endif
#Packages that should not be installed in QMAA are enabled here
ifneq ($(TARGET_IS_HEADLESS),true)
#MM_AUDIO product packages
MM_AUDIO := libcapiv2uvvendor
MM_AUDIO += sound_trigger.primary.$(TARGET_BOARD_PLATFORM)
MM_AUDIO += libadm
MM_AUDIO += libAlacSwDec
MM_AUDIO += libApeSwDec
MM_AUDIO += libcapiv2svacnnvendor
MM_AUDIO += libcapiv2svarnnvendor
MM_AUDIO += libcapiv2udk7vendor
MM_AUDIO += libdsd2pcm
MM_AUDIO += libFlacSwDec
MM_AUDIO += libasphere
MM_AUDIO += libqcompostprocbundle
MM_AUDIO += libqcomvisualizer
MM_AUDIO += libqcomvoiceprocessing
MM_AUDIO += libshoebox
MM_AUDIO += libbatterylistener
MM_AUDIO += audioflacapp
MM_AUDIO += libvolumelistener

#KERNEL_TESTS
#KERNEL_TESTS := mm-audio-native-test

AUDIO_GENERIC_MODULES += $(MM_AUDIO)
#AUDIO_GENERIC_MODULES += $(KERNEL_TESTS)

endif
