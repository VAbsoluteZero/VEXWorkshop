# Install script for directory: E:/VAbsoluteZero/GameOfLife/third_party/SDL2

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/VEXGameOfLife")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Debug/SDL2d.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Release/SDL2.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/RELWITHDEBINFO/SDL2.lib")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Debug/SDL2d.dll")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Release/SDL2.dll")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/RELWITHDEBINFO/SDL2.dll")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Debug/SDL2maind.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Release/SDL2main.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/RELWITHDEBINFO/SDL2main.lib")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Debug/SDL2-staticd.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/Release/SDL2-static.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/RELWITHDEBINFO/SDL2-static.lib")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2Targets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2Targets.cmake"
         "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2Targets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2Targets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2Targets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2Targets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2Targets-debug.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2Targets-relwithdebinfo.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2Targets-release.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2mainTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2mainTargets.cmake"
         "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2mainTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2mainTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2mainTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2mainTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2mainTargets-debug.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2mainTargets-relwithdebinfo.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2mainTargets-release.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2staticTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2staticTargets.cmake"
         "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2staticTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2staticTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/cmake/SDL2staticTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2staticTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2staticTargets-debug.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2staticTargets-relwithdebinfo.cmake")
  endif()
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/CMakeFiles/Export/cmake/SDL2staticTargets-release.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDevelx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/cmake" TYPE FILE FILES
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/SDL2Config.cmake"
    "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/SDL2ConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SDL2" TYPE FILE FILES
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_assert.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_atomic.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_audio.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_bits.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_blendmode.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_clipboard.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_android.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_emscripten.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_iphoneos.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_macosx.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_minimal.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_os2.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_pandora.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_windows.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_config_winrt.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_copying.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_cpuinfo.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_egl.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_endian.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_error.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_events.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_filesystem.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_gamecontroller.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_gesture.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_haptic.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_hidapi.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_hints.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_joystick.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_keyboard.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_keycode.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_loadso.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_locale.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_log.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_main.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_messagebox.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_metal.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_misc.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_mouse.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_mutex.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_name.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengl.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengl_glext.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles2.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles2_gl2.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles2_gl2ext.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles2_gl2platform.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_opengles2_khrplatform.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_pixels.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_platform.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_power.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_quit.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_rect.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_render.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_rwops.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_scancode.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_sensor.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_shape.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_stdinc.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_surface.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_system.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_syswm.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_assert.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_common.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_compare.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_crc32.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_font.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_fuzzer.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_harness.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_images.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_log.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_md5.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_memory.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_test_random.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_thread.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_timer.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_touch.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_types.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_version.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_video.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/SDL_vulkan.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/begin_code.h"
    "E:/VAbsoluteZero/GameOfLife/third_party/SDL2/include/close_code.h"
    "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/include/SDL_config.h"
    "E:/VAbsoluteZero/GameOfLife/cmake_out/MSVCDebug/third_party/SDL2/include/SDL_revision.h"
    )
endif()

