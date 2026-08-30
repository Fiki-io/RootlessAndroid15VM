# RootlessAndroid15VM

> **Rootless Android 15 Virtual Machine & Container Engine for Android**  
> Run a full Android 15 environment on top of Android without Root, without pKVM, without QEMU, and without Dual Boot.

---

## 🌟 Key Features

- **No Root Required:** Operates entirely in Android user space using advanced `ptrace` system call interception.
- **Android 15 (ARM64) Support:** Tailored for Android 15 AOSP/GSI with flattened APEX modules (Bionic, ART) and patched `init.rc`.
- **16KB Page Size Compliant:** Fully compatible with Android 15 kernels on Snapdragon 8 Gen 3/4 and Pixel 9 (`-Wl,-z,max-page-size=16384`).
- **High-Performance Memory I/O:** Uses `process_vm_readv` and `process_vm_writev` for microsecond-level path translation and memory transfers.
- **Linux Multi-Touch Protocol Type B:** Precise finger tracking and hardware keycode injection.
- **Hardware Graphics & Framebuffer:** 60 FPS shared memory ring-buffer (`GrallocServer`), OpenGL ES 2.0/3.0 texture blitter (`EGLRenderer`), and `/dev/graphics/fb0` ioctl emulation.
- **Audio & DNS Proxy:** Built-in low-latency audio via Android NDK **AAudio** and `/dev/socket/dnsproxyd` DNS resolution for full internet connectivity.
- **Anti-Kill Foreground Service:** Persistent foreground service with WakeLock protection.
- **Shared Storage:** Direct binding of `/sdcard/Download` to the host device storage.

---

## 📂 Architecture Overview

```text
RootlessAndroid15VM/
├── setup_rom.sh                         # Automated ROM extraction, patching & packaging pipeline
├── patch_rootfs.py                      # Python deep-patcher for Android 15 GSI
├── build_cli.sh                         # Standalone C++ test runner build script
│
├── engine/                              # NDK C++ Core Engine
│   ├── CMakeLists.txt                   # CMake build script for libvm_engine.so
│   ├── jni_bridge.cpp                   # JNI bridge connecting Java UI to C++ engine
│   ├── main_cli.cpp                     # Standalone CLI test runner
│   │
│   ├── core/                            # Kernel & Syscall Interception
│   │   ├── path_resolver.cpp/hpp        # Virtual chroot + Shared Storage (/sdcard/Download)
│   │   ├── memory_manager.cpp/hpp       # High-speed memory I/O (process_vm_readv / writev)
│   │   ├── process_tree.cpp/hpp         # Zygote multi-process & thread tracker
│   │   └── syscall_dispatcher.cpp/hpp   # 30+ Linux/Android Syscall Handlers
│   │
│   ├── ipc/                             # Inter-Process Communication
│   │   ├── property_service.cpp/hpp     # Android Property Service daemon (/dev/socket/property_service)
│   │   ├── dns_proxy.cpp/hpp            # DNS proxy daemon (/dev/socket/dnsproxyd & resolv.conf)
│   │   └── virtual_binder.cpp/hpp       # Virtual Binder IOCTL Handler & ASharedMemory
│   │
│   ├── audio/                           # Sound System
│   │   └── virtual_audio.cpp/hpp        # Virtual PCM Sink (/dev/snd/pcmC0D0p) -> AAudio Bridge
│   │
│   ├── display/                         # Graphics & Display
│   │   ├── virtual_display.cpp/hpp      # ANativeWindow Bridge for SurfaceView
│   │   ├── gralloc_server.cpp/hpp       # 60 FPS Shared Memory Ring-Buffer
│   │   ├── virtual_framebuffer.cpp/hpp  # /dev/graphics/fb0 IOCTL Emulation
│   │   └── egl_renderer.cpp/hpp         # OpenGL ES 2.0 Texture Blitter
│   │
│   ├── input/                           # Input Controls
│   │   └── virtual_input.cpp/hpp        # Linux Multi-Touch Type B & Keys
│   │
│   └── guest/                           # Guest-Side Orchestration
│       └── guest_init_orchestrator.cpp  # Supervisor for servicemanager, surfaceflinger, zygote
│
└── app/                                 # Android Studio App Module (Java/Kotlin)
    ├── build.gradle                     # SDK 35 Target, NDK CMake Integration
    └── src/main/java/com/vm/engine/
        ├── VMEngine.java                # JNI Native Controller
        ├── VMSurfaceView.java           # Custom SurfaceView Renderer
        ├── ROMManager.java              # ROM Downloader and Extractor
        ├── VMForegroundService.java     # Anti-Kill Foreground Service
        └── MainActivity.java            # Main User Interface
```

---

## 📦 ROM Download

The pre-patched, ready-to-run **Android 15 ARM64 RootFS** (`android15_rootfs.tar.xz`) is available in the **[Releases](https://github.com/Fiki-io/RootlessAndroid15VM/releases)** tab.

---

## 🚀 Getting Started

### 1. Build and Run via Android Studio
1. Open this repository in Android Studio.
2. Ensure you have the Android NDK and CMake installed via SDK Manager.
3. Build and install the APK to your physical ARM64 Android device.
4. Place `android15_rootfs.tar.xz` into your device's `/sdcard/Download/` folder (or download via the in-app installer) and press **Start**.

### 2. Standalone CLI Test Runner (Linux / Termux)
```bash
# Compile the CLI runner
./build_cli.sh

# Run the virtual environment
./vm_cli_runner ./rootfs /init_guest.sh
```

---

## 📄 License
This project is licensed under the Apache License 2.0.
