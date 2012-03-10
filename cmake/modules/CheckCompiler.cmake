set(BITNESS 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITNESS 64)
  message("-- detected 64bit")
else()
  message("-- detected 32bit")
endif()

if(UNIX)
  include(${CMAKE_PLATFORM_PATH}/unix/platform.cmake)
elseif(WIN32)
  include(${CMAKE_PLATFORM_PATH}/win32/platform.cmake)
endif()
