include(${CMAKE_ANDROID_NDK}/build/cmake/platforms.cmake)

function(adjust_api_level api_level result_name)
  # If no platform version was chosen by the user, default to the minimum
  # version supported by this NDK.
  if(NOT api_level)
    message(STATUS
      "ANDROID_PLATFORM not set. Defaulting to minimum supported version "
      "${NDK_MIN_PLATFORM_LEVEL}.")

    set(api_level "android-${NDK_MIN_PLATFORM_LEVEL}")
  endif()

  if(api_level STREQUAL "latest")
    message(STATUS
      "Using latest available ANDROID_PLATFORM: ${NDK_MAX_PLATFORM_LEVEL}.")
    set(api_level "android-${NDK_MAX_PLATFORM_LEVEL}")
  endif()

  string(REPLACE "android-" "" result ${api_level})

  # Aliases defined by meta/platforms.json include codename aliases for platform
  # API levels as well as cover any gaps in platforms that may not have had NDK
  # APIs.
  if(NOT "${NDK_PLATFORM_ALIAS_${result}}" STREQUAL "")
    message(STATUS
      "${api_level} is an alias for ${NDK_PLATFORM_ALIAS_${result}}. Adjusting "
      "ANDROID_PLATFORM to match.")
    set(api_level "${NDK_PLATFORM_ALIAS_${result}}")
    string(REPLACE "android-" "" result ${api_level})
  endif()

  # Pull up to the minimum supported version if an old API level was requested.
  if(result LESS NDK_MIN_PLATFORM_LEVEL)
    message(STATUS
      "${api_level} is unsupported. Using minimum supported version "
      "${NDK_MIN_PLATFORM_LEVEL}.")
    set(api_level "android-${NDK_MIN_PLATFORM_LEVEL}")
    string(REPLACE "android-" "" result ${api_level})
  endif()

  # And for LP64 we need to pull up to 21. No diagnostic is provided here
  # because minSdkVersion < 21 is valid for the project even though it may not
  # be for this ABI.
  if(ANDROID_ABI MATCHES "64(-v8a)?$" AND result LESS 21)
    message(STATUS
      "android-${result} is not supported for ${ANDROID_ABI}. Using minimum "
      "supported LP64 version 21.")
    set(api_level android-21)
    set(result 21)
  endif()

  # ANDROID_PLATFORM beyond the maximum is an error. The correct way to specify
  # the latest version is ANDROID_PLATFORM=latest.
  if(result GREATER NDK_MAX_PLATFORM_LEVEL)
    message(SEND_ERROR
      "${api_level} is above the maximum supported version "
      "${NDK_MAX_PLATFORM_LEVEL}. Choose a supported API level or set "
      "ANDROID_PLATFORM to \"latest\".")
  endif()

  set(${result_name} ${result} PARENT_SCOPE)
endfunction()
