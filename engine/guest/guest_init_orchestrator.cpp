#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>

/**
 * GuestInitOrchestrator: Pengganti /system/bin/init untuk container userspace Android 15.
 * Tugas: Menjalankan dan mensupervisi 4 daemon inti AOSP secara berurutan:
 * 1. servicemanager  (AIDL Service Registry)
 * 2. surfaceflinger  (Window & Frame Compositor)
 * 3. zygote          (ART VM Runtime & app_process64)
 * 4. system_server   (ActivityManager, WindowManager, dll)
 */

struct DaemonProcess {
    std::string name;
    std::string bin_path;
    std::vector<std::string> args;
    pid_t pid = -1;
    bool critical = true;
};

static std::vector<DaemonProcess> g_daemons;
static bool g_running = true;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = false;
    }
}

void setup_environment() {
    // Konfigurasi Environment Variable Inti Android 15
    setenv("PATH", "/apex/com.android.runtime/bin:/apex/com.android.art/bin:/system/bin:/system/xbin", 1);
    setenv("ANDROID_ROOT", "/system", 1);
    setenv("ANDROID_DATA", "/data", 1);
    setenv("ANDROID_STORAGE", "/storage", 1);
    setenv("ANDROID_ART_ROOT", "/apex/com.android.art", 1);
    setenv("ANDROID_RUNTIME_ROOT", "/apex/com.android.runtime", 1);
    setenv("ANDROID_TZDATA_ROOT", "/apex/com.android.tzdata", 1);
    setenv("ANDROID_I18N_ROOT", "/apex/com.android.i18n", 1);

    // Boot classpath Android 15
    std::string bcp = 
        "/apex/com.android.art/javalib/core-oj.jar:"
        "/apex/com.android.art/javalib/core-libart.jar:"
        "/apex/com.android.art/javalib/okhttp.jar:"
        "/apex/com.android.art/javalib/bouncycastle.jar:"
        "/apex/com.android.art/javalib/apache-xml.jar:"
        "/system/framework/framework.jar:"
        "/system/framework/framework-graphics.jar:"
        "/system/framework/framework-location.jar:"
        "/system/framework/ext.jar:"
        "/system/framework/telephony-common.jar:"
        "/system/framework/voip-common.jar:"
        "/system/framework/ims-common.jar";

    setenv("BOOTCLASSPATH", bcp.c_str(), 1);
}

pid_t start_daemon(DaemonProcess& d) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[-] Gagal fork daemon: " << d.name << "\n";
        return -1;
    }

    if (pid == 0) {
        // Child Process
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(d.bin_path.c_str()));
        for (const auto& arg : d.args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        std::cout << "[GUEST_INIT] Menjalankan " << d.name << " (" << d.bin_path << ")...\n";
        execv(d.bin_path.c_str(), c_args.data());

        // Jika execv gagal
        std::cerr << "[-] Execv gagal untuk " << d.name << "\n";
        _exit(1);
    }

    d.pid = pid;
    return pid;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::cout << "\033[1;32m====================================================\033[0m\n";
    std::cout << "\033[1;32m=== Android 15 Guest Init Orchestrator (Rootless) ===\033[0m\n";
    std::cout << "\033[1;32m====================================================\033[0m\n";

    setup_environment();

    // 1. Definisikan Daemon Inti Android 15
    g_daemons = {
        {
            "servicemanager",
            "/system/bin/servicemanager",
            {},
            -1,
            true
        },
        {
            "surfaceflinger",
            "/system/bin/surfaceflinger",
            {},
            -1,
            true
        },
        {
            "zygote64",
            "/system/bin/app_process64",
            {"/system/bin", "--zygote", "--start-system-server"},
            -1,
            true
        }
    };

    // 2. Jalankan Daemon secara berurutan dengan jeda stabilisasi
    for (auto& d : g_daemons) {
        std::cout << "[*] Memulai service: " << d.name << "\n";
        start_daemon(d);
        usleep(500000); // 500ms jeda antar service agar Binder terinisialisasi
    }

    std::cout << "\033[1;32m[+] Seluruh service Android 15 telah berjalan. Memasuki pengawasan proses...\033[0m\n";

    // 3. Loop Pengawasan (Supervisor Loop)
    while (g_running) {
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);

        if (exited_pid > 0) {
            for (auto& d : g_daemons) {
                if (d.pid == exited_pid) {
                    std::cerr << "\033[1;33m[!] Service " << d.name << " (PID " << exited_pid << ") berhenti. Memulai ulang...\033[0m\n";
                    usleep(1000000); // Tunggu 1 detik sebelum restart
                    start_daemon(d);
                }
            }
        }

        sleep(1);
    }

    // 4. Shutdown Bersih
    std::cout << "[*] Menghentikan seluruh service guest Android 15...\n";
    for (auto& d : g_daemons) {
        if (d.pid > 0) {
            kill(d.pid, SIGTERM);
        }
    }

    return 0;
}
