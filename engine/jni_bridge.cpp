#include <jni.h>
#include <string>
#include <thread>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#include "core/path_resolver.hpp"
#include "core/process_tree.hpp"
#include "core/syscall_dispatcher.hpp"
#include "ipc/property_service.hpp"
#include "ipc/dns_proxy.hpp"
#include "ipc/virtual_binder.hpp"
#include "audio/virtual_audio.hpp"
#include "input/virtual_input.hpp"
#include "display/virtual_display.hpp"
#include "display/gralloc_server.hpp"
#include "display/virtual_framebuffer.hpp"

#define TAG "VMEngine_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static PathResolver* g_resolver = nullptr;
static ProcessTree* g_process_tree = nullptr;
static SyscallDispatcher* g_dispatcher = nullptr;
static PropertyService* g_prop_service = nullptr;
static DNSProxy* g_dns_proxy = nullptr;
static VirtualBinder* g_binder = nullptr;
static VirtualAudio* g_audio = nullptr;
static VirtualInput* g_input = nullptr;
static VirtualDisplay* g_display = nullptr;
static GrallocServer* g_gralloc = nullptr;
static VirtualFramebuffer* g_fb = nullptr;

static std::thread* g_vm_thread = nullptr;
static pid_t g_guest_pid = -1;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_vm_engine_VMEngine_nativeStartVM(JNIEnv* env, jobject /* this */, jstring rootfsPath, jstring entrypoint) {
    const char* c_rootfs = env->GetStringUTFChars(rootfsPath, nullptr);
    const char* c_entry = env->GetStringUTFChars(entrypoint, nullptr);

    std::string rootfs_str(c_rootfs);
    std::string entry_str(c_entry);

    env->ReleaseStringUTFChars(rootfsPath, c_rootfs);
    env->ReleaseStringUTFChars(entrypoint, c_entry);

    LOGI("Menginisialisasi Full Production Engine dengan RootFS: %s", rootfs_str.c_str());

    // 1. Core Subsystems
    g_resolver = new PathResolver(rootfs_str);
    g_process_tree = new ProcessTree();

    // 2. IPC, Property & DNS
    g_prop_service = new PropertyService();
    g_prop_service->load_properties_file(rootfs_str + "/system/build.prop");
    g_prop_service->start(rootfs_str);

    g_dns_proxy = new DNSProxy();
    g_dns_proxy->start(rootfs_str);

    g_binder = new VirtualBinder();
    g_binder->setup_binder_endpoints(rootfs_str);

    // 3. Audio, Input & Display
    g_audio = new VirtualAudio();
    g_audio->init_audio(rootfs_str, 48000, 2);

    g_input = new VirtualInput();
    g_input->init_device(rootfs_str + "/dev/input_event0");

    if (g_display == nullptr) {
        g_display = new VirtualDisplay();
    }
    g_gralloc = new GrallocServer();
    g_gralloc->init(1080, 2400);

    g_fb = new VirtualFramebuffer(g_gralloc);
    g_fb->init_fb_device(rootfs_str, 1080, 2400);

    // 4. Dispatcher
    g_dispatcher = new SyscallDispatcher(g_resolver, g_process_tree, g_binder, g_fb);

    // 5. Background Dispatcher Thread
    g_vm_thread = new std::thread([entry_str]() {
        LOGI("Meluncurkan Android 15 init runner: %s", entry_str.c_str());
        std::vector<std::string> args;
        g_guest_pid = g_dispatcher->spawn_guest(entry_str, args);

        if (g_guest_pid > 0) {
            LOGI("Guest PID %d aktif. Memasuki Syscall Dispatcher Event Loop...", g_guest_pid);
            g_dispatcher->run_loop(g_guest_pid);
            LOGI("Event loop guest selesai.");
        } else {
            LOGE("Gagal meluncurkan guest init!");
        }
    });

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_vm_engine_VMEngine_nativeSetSurface(JNIEnv* env, jobject /* this */, jobject surface) {
    if (g_display == nullptr) {
        g_display = new VirtualDisplay();
    }

    if (surface != nullptr) {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        g_display->set_native_window(window);
    } else {
        g_display->release();
    }
}

JNIEXPORT void JNICALL
Java_com_vm_engine_VMEngine_nativeSendTouchEvent(JNIEnv* /* env */, jobject /* this */, jint action, jfloat x, jfloat y) {
    if (g_input != nullptr) {
        g_input->send_touch(action, 0, x, y);
    }
}

JNIEXPORT void JNICALL
Java_com_vm_engine_VMEngine_nativeStopVM(JNIEnv* /* env */, jobject /* this */) {
    LOGI("Menghentikan seluruh subsistem VM...");
    if (g_process_tree) {
        g_process_tree->kill_all();
    }
    if (g_prop_service) {
        g_prop_service->stop();
        delete g_prop_service;
        g_prop_service = nullptr;
    }
    if (g_dns_proxy) {
        g_dns_proxy->stop();
        delete g_dns_proxy;
        g_dns_proxy = nullptr;
    }
    if (g_audio) {
        g_audio->stop();
        delete g_audio;
        g_audio = nullptr;
    }
    if (g_vm_thread && g_vm_thread->joinable()) {
        g_vm_thread->join();
        delete g_vm_thread;
        g_vm_thread = nullptr;
    }
    delete g_dispatcher; g_dispatcher = nullptr;
    delete g_process_tree; g_process_tree = nullptr;
    delete g_resolver; g_resolver = nullptr;
    delete g_binder; g_binder = nullptr;
    delete g_input; g_input = nullptr;
    delete g_fb; g_fb = nullptr;
    delete g_gralloc; g_gralloc = nullptr;
    LOGI("Semua subsistem VM berhasil dibersihkan.");
}

} // extern "C"
