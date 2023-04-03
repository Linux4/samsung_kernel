#
# for huaqin misc info
#

#F52 code for SR-QN3979-01-94 by SunXunou at 20210202 start
obj-$(CONFIG_HQ_SIM_TRAY_CHECK)   += sim_tray/
#F52 code for SR-QN3979-01-94 by SunXunou at 20210202 end
obj-$(CONFIG_HW_INFO)   += hw_info/
#F52 code for dump display by yanghui at 2021/03/09 start
obj-$(CONFIG_RECORD_CRASH_INFO)   += crash_record/
#F52 code for dump display by yanghui at 2021/03/09 end

#F52 code for trigger crash with key by yutao at 2021/04/22 start
#obj-$(CONFIG_TRIGGER_CRASH_BY_KEY)     += combine_key_trigger.o
ifdef HQ_FACTORY_BUILD
obj-y   += combine_key_trigger.o
endif
#F52 code for trigger crash with key by yutao at 2021/04/22 end



