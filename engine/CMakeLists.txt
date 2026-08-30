cmake_minimum_required(VERSION 3.22.1)

project("vm_engine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Kompatibilitas Ukuran Page Memori 16KB untuk Android 15 ARM64
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/display
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/input
)

add_library(vm_engine SHARED
    core/path_resolver.cpp
    core/memory_manager.cpp
    core/process_tree.cpp
    core/syscall_dispatcher.cpp
    ipc/property_service.cpp
    ipc/dns_proxy.cpp
    ipc/virtual_binder.cpp
    audio/virtual_audio.cpp
    display/virtual_display.cpp
    display/gralloc_server.cpp
    display/virtual_framebuffer.cpp
    display/egl_renderer.cpp
    input/virtual_input.cpp
    jni_bridge.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles2-lib GLESv2)
find_library(aaudio-lib aaudio)

target_link_libraries(vm_engine
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles2-lib}
    ${aaudio-lib}
)
