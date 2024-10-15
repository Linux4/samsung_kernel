# SurfaceFlinger
PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.vsync_event_phase_offset_ns=0 \
    ro.surface_flinger.vsync_sf_event_phase_offset_ns=0 \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
    ro.surface_flinger.running_without_sync_framework=false \
    ro.surface_flinger.use_color_management=true\
    ro.surface_flinger.has_wide_color_display=true \
    ro.surface_flinger.has_HDR_display = 1 \
    persist.sys.sf.color_saturation=1.0 \
    debug.sf.auto_latch_unsignaled=0 \
    debug.sf.latch_unsignaled=1 \
    ro.surface_flinger.use_content_detection_for_refresh_rate=true \
    ro.surface_flinger.set_touch_timer_ms=300 \
    ro.surface_flinger.set_idle_timer_ms=250 \
    ro.surface_flinger.set_display_power_timer_ms=200 \
    debug.sf.high_fps_late_app_phase_offset_ns=0 \
    debug.sf.high_fps_late_sf_phase_offset_ns=0

# Screen property
#PRODUCT_PROPERTY_OVERRIDES += \
#ro.sf.lcd_density=560

PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.protected_contents=1
