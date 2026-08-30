#!/usr/bin/env bash
# Kompilasi Production-Grade Standalone CLI Runner
set -e

echo "Mengompilasi Full Production-Grade VM Engine CLI Runner..."

CXX=${CXX:-g++}

$CXX -std=c++17 -O2 \
    engine/main_cli.cpp \
    engine/core/path_resolver.cpp \
    engine/core/memory_manager.cpp \
    engine/core/process_tree.cpp \
    engine/core/syscall_dispatcher.cpp \
    engine/ipc/property_service.cpp \
    engine/ipc/dns_proxy.cpp \
    engine/ipc/virtual_binder.cpp \
    engine/audio/virtual_audio.cpp \
    engine/display/gralloc_server.cpp \
    engine/display/virtual_framebuffer.cpp \
    engine/input/virtual_input.cpp \
    -Iengine \
    -Iengine/core \
    -Iengine/ipc \
    -Iengine/audio \
    -Iengine/display \
    -Iengine/input \
    -lpthread \
    -o vm_cli_runner

echo "Selesai! Binary executable produksi dibuat: ./vm_cli_runner"
