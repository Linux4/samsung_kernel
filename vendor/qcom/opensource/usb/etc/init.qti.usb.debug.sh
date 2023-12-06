#!/vendor/bin/sh
# Copyright (c) 2021 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
#  Changes from Qualcomm Innovation Center are provided under the following license:
#
#  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted (subject to the limitations in the
#  disclaimer below) provided that the following conditions are met:
#
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided
#        with the distribution.
#
#      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
#        contributors may be used to endorse or promote products derived
#        from this software without specific prior written permission.
#
#  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
#  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
#  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
#  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
#  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#prop_enabled=`getprop persist.vendor.usb.enable_ftrace 0`

# bail out if its perf config
#if [ "$prop_enabled" == "0" -a ! -d /sys/module/msm_rtb ]; then
#    return
#fi

# Enable various ftrace debugging events for USB
tracefs=/sys/kernel/tracing

if [ -d $tracefs ]; then
    cd $tracefs

    # global kprobe events
    echo 'p:usb_gadget/p_config_usb_cfg_link_0 config_usb_cfg_link cfg=+0(+0($arg1)):string func=+0(+0($arg2)):string' >> kprobe_events
    echo 'r:usb_gadget/r_config_usb_cfg_link_0 config_usb_cfg_link ret=$retval:s32' >> kprobe_events
    echo 'p:usb_gadget/p_config_usb_cfg_unlink_0 config_usb_cfg_unlink cfg=+0(+0($arg1)):string func=+0(+0($arg2)):string' >> kprobe_events
    echo 'p:usb_gadget/p_gadget_dev_desc_UDC_store_0 gadget_dev_desc_UDC_store udc=+0($arg2):string' >> kprobe_events
    echo 'r:usb_gadget/r_gadget_dev_desc_UDC_store_0 gadget_dev_desc_UDC_store ret=$retval:s32' >> kprobe_events
    echo 'p:usb_gadget/p_unregister_gadget_item_0 unregister_gadget_item gadget=+0(+0($arg1)):string' >> kprobe_events

    # usb instances
    mkdir instances/usb
    cd instances/usb

    # dwc3
    echo 1 > events/dwc3/dwc3_complete_trb/enable
    echo 1 > events/dwc3/dwc3_ctrl_req/enable
    echo 1 > events/dwc3/dwc3_ep_dequeue/enable
    echo 1 > events/dwc3/dwc3_ep_queue/enable
    echo 1 > events/dwc3/dwc3_gadget_ep_cmd/enable
    echo 1 > events/dwc3/dwc3_gadget_ep_disable/enable
    echo 1 > events/dwc3/dwc3_gadget_ep_enable/enable
    echo 1 > events/dwc3/dwc3_gadget_giveback/enable
    echo 1 > events/dwc3/dwc3_prepare_trb/enable
    echo 1 > events/dwc3/dwc3_event/enable
    echo 1 > events/dwc3/dwc3_alloc_request/enable
    echo 1 > events/dwc3/dwc3_free_request/enable
    echo 1 > events/dwc3/dwc3_gadget_generic_cmd/enable

    # ucsi
    echo 1 > events/ucsi/ucsi_connector_change/enable
    echo 1 > events/ucsi/ucsi_reset_ppm/enable
    echo 1 > events/ucsi/ucsi_run_command/enable

    # USB gadget
    echo 1 > events/gadget/usb_gadget_disconnect/enable
    echo 1 > events/gadget/usb_gadget_connect/enable
    echo 1 > events/gadget/usb_gadget_vbus_draw/enable

    # usb instance kprobe events
    echo 1 > events/usb_gadget/enable

    #DWC3 core runtime
    echo 'name~"a600000.*"' > events/rpm/filter
    echo 1 > events/rpm/rpm_resume/enable
    echo 1 > events/rpm/rpm_suspend/enable
    echo 1 > events/rpm/rpm_return_int/enable

    #xhci
    mkdir $tracefs/instances/usb_xhci
    echo 1 > $tracefs/instances/usb_xhci/events/xhci-hcd/enable

    echo 1 > $tracefs/instances/usb_xhci/tracing_on
    echo 1 > tracing_on

    #dwc3_readl_writel
    mkdir $tracefs/instances/dwc3_rw_traces
    echo 1 > $tracefs/instances/dwc3_rw_traces/events/dwc3/dwc3_readl/enable
    echo 1 > $tracefs/instances/dwc3_rw_traces/events/dwc3/dwc3_writel/enable
fi
