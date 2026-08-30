#!/usr/bin/env python3
"""
Android 15 RootFS Deep Patcher for Rootless Android-on-Android Container
Author: Antigravity Project
Description:
  Automates the complete extraction, APEX flattening, init.rc tweaking,
  property injection, and packaging of Android 15 GSI/AOSP for rootless execution.
"""

import os
import sys
import shutil
import subprocess
import re
from pathlib import Path

def log(msg):
    print(f"\033[1;32m[+]\033[0m {msg}")

def warn(msg):
    print(f"\033[1;33m[!]\033[0m {msg}")

def err(msg):
    print(f"\033[1;31m[-]\033[0m {msg}")

class Android15RootfsPatcher:
    def __init__(self, rootfs_dir: str):
        self.rootfs = Path(rootfs_dir).resolve()
        if not self.rootfs.exists() or not self.rootfs.is_dir():
            raise ValueError(f"RootFS directory '{rootfs_dir}' does not exist!")

    def run_all_patches(self):
        log(f"Memulai proses deep patching pada: {self.rootfs}")
        self.fix_symlinks()
        self.create_skeleton_dirs()
        self.flatten_apex_packages()
        self.patch_bionic_and_linker()
        self.patch_init_rc_files()
        self.patch_system_properties()
        self.create_entrypoint_script()
        log("Semua patch Android 15 berhasil diaplikasikan!")

    def fix_symlinks(self):
        """Memperbaiki symlink absolut agar menjadi relatif terhadap rootfs"""
        log("Memperbaiki symlink rootfs agar kompatibel dengan container...")
        rootfs_str = str(self.rootfs)
        for root, dirs, files in os.walk(self.rootfs, followlinks=False):
            for name in dirs + files:
                full_path = Path(root) / name
                if full_path.is_symlink():
                    try:
                        target = os.readlink(full_path)
                        if target.startswith(rootfs_str):
                            rel_target = os.path.relpath(target, full_path.parent)
                            full_path.unlink()
                            full_path.symlink_to(rel_target)
                    except Exception as e:
                        pass

    def create_skeleton_dirs(self):
        """Membuat struktur direktori standar Linux/Android yang wajib ada"""
        log("Memeriksa dan membuat skeleton folder sistem (/dev, /proc, /sys, /data, /apex, dll)...")
        dirs = [
            "dev", "proc", "sys", "data", "apex", "mnt", "tmp",
            "system", "vendor", "product", "system_ext",
            "data/data", "data/app", "data/system", "data/local/tmp",
            "apex/com.android.runtime", "apex/com.android.art"
        ]
        for d in dirs:
            p = self.rootfs / d
            try:
                if not p.exists() and not p.is_symlink():
                    p.mkdir(parents=True, exist_ok=True)
            except Exception as e:
                pass

    def flatten_apex_packages(self):
        """
        Di Android 15, Bionic libc dan ART Runtime dikemas dalam file .apex.
        Di container rootless tanpa apexd daemon, paket .apex harus diekstrak (flattened).
        """
        log("Memeriksa dan mengekstrak paket APEX (Bionic, ART, Conscrypt)...")
        apex_dir = self.rootfs / "system" / "apex"
        if not apex_dir.exists():
            apex_dir = self.rootfs / "apex"

        if apex_dir.exists() and apex_dir.is_dir():
            for apex_file in apex_dir.glob("*.apex"):
                package_name = apex_file.stem
                target_apex = self.rootfs / "apex" / package_name
                log(f"Mengekstrak APEX: {apex_file.name} -> {target_apex}")
                target_apex.mkdir(parents=True, exist_ok=True)
                
                try:
                    subprocess.run(["7z", "x", str(apex_file), f"-o{target_apex}", "-y"],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
                    
                    payload_img = target_apex / "apex_payload.img"
                    if payload_img.exists():
                        subprocess.run(["7z", "x", str(payload_img), f"-o{target_apex}", "-y"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
                        payload_img.unlink(missing_ok=True)
                except Exception as e:
                    warn(f"Gagal ekstrak otomatis {apex_file.name}: {e}")

    def patch_bionic_and_linker(self):
        """Membuat symlink agar Dynamic Linker menemukan libc.so di /apex"""
        log("Menyiapkan symlink Linker dan Bionic runtime...")
        lib64_dir = self.rootfs / "system" / "lib64"
        bionic_apex_lib64 = self.rootfs / "apex" / "com.android.runtime" / "lib64" / "bionic"

        if bionic_apex_lib64.exists() and lib64_dir.exists():
            for lib in ["libc.so", "libm.so", "libdl.so"]:
                src = bionic_apex_lib64 / lib
                dst = lib64_dir / lib
                if src.exists() and not dst.exists():
                    try:
                        dst.symlink_to(f"/apex/com.android.runtime/lib64/bionic/{lib}")
                        log(f"Symlinked {lib} -> {dst}")
                    except Exception:
                        try:
                            shutil.copy2(src, dst)
                        except Exception:
                            pass

    def patch_init_rc_files(self):
        """Mematikan baris SELinux, cgroups, vold, dan driver hardware fisik pada file *.rc"""
        log("Melakukan deep-patch pada file init.rc dan /system/etc/init/*.rc...")
        
        dangerous_patterns = [
            re.compile(r'^\s*(mount_all\s+.*)', re.MULTILINE),
            re.compile(r'^\s*(mount\s+cgroup.*)', re.MULTILINE),
            re.compile(r'^\s*(mount\s+bpf.*)', re.MULTILINE),
            re.compile(r'^\s*(setcon\s+.*)', re.MULTILINE),
            re.compile(r'^\s*(setenforce\s+.*)', re.MULTILINE),
            re.compile(r'^\s*(restorecon\s+.*)', re.MULTILINE),
            re.compile(r'^\s*(restorecon_recursive\s+.*)', re.MULTILINE),
            re.compile(r'^\s*(write\s+/proc/sys/kernel/.*)', re.MULTILINE),
            re.compile(r'^\s*(write\s+/sys/fs/selinux/.*)', re.MULTILINE),
            re.compile(r'^\s*(write\s+/dev/cpuset/.*)', re.MULTILINE),
            re.compile(r'^\s*(write\s+/dev/stune/.*)', re.MULTILINE),
            re.compile(r'^\s*(mkdir\s+/dev/cpuctl.*)', re.MULTILINE),
            re.compile(r'^\s*(mkdir\s+/dev/cpuset.*)', re.MULTILINE),
            re.compile(r'^\s*(swapon_all\s+.*)', re.MULTILINE),
        ]

        hardware_services_to_disable = [
            "vold", "gatekeeperd", "keymint", "vendor.ril-daemon",
            "wificond", "bluetooth", "netd", "tombstoned"
        ]

        patched_count = 0
        for rc_path in self.rootfs.glob("**/*.rc"):
            try:
                content = rc_path.read_text(encoding="utf-8", errors="ignore")
                orig_content = content

                for pattern in dangerous_patterns:
                    content = pattern.sub(r'# [PATCHED_ROOTLESS] \1', content)

                for srv in hardware_services_to_disable:
                    srv_pattern = re.compile(rf'^\s*service\s+{srv}\s+', re.MULTILINE)
                    if srv_pattern.search(content):
                        content = srv_pattern.sub(f'service {srv} [DISABLED_BY_PATCHER] ', content)

                if content != orig_content:
                    rc_path.write_text(content, encoding="utf-8")
                    patched_count += 1

            except Exception as e:
                warn(f"Gagal mem-patch {rc_path}: {e}")

        log(f"Total file .rc yang berhasil di-patch: {patched_count}")

    def patch_system_properties(self):
        """Menambahkan properties konfigurasi khusus container di build.prop"""
        log("Menyesuaikan system properties (build.prop & default.prop)...")
        prop_files = [
            self.rootfs / "system" / "build.prop",
            self.rootfs / "system" / "etc" / "prop.default",
            self.rootfs / "prop.default"
        ]

        custom_props = [
            "\n# --- ROOTLESS CONTAINER PATCHED PROPS ---",
            "ro.debuggable=1",
            "ro.secure=0",
            "ro.adb.secure=0",
            "ro.allow.mock.location=1",
            "ro.config.low_ram=false",
            "persist.sys.timezone=Asia/Jakarta",
            "ro.sf.lcd_density=420",
            "debug.sf.nobootanimation=1",
            "ro.hardware.gralloc=default",
            "ro.hardware.hwcomposer=default",
            "ro.vendor.build.security_patch=2026-08-01",
            "sys.usb.config=none",
            "# ----------------------------------------\n"
        ]
        props_text = "\n".join(custom_props)

        for prop_file in prop_files:
            if prop_file.exists() and not prop_file.is_symlink():
                try:
                    with open(prop_file, "a", encoding="utf-8") as f:
                        f.write(props_text)
                    log(f"Injected custom properties ke: {prop_file.relative_to(self.rootfs)}")
                except Exception as e:
                    warn(f"Gagal menulis properties ke {prop_file}: {e}")

    def create_entrypoint_script(self):
        """Membuat script entrypoint minimal untuk inisialisasi awal container"""
        log("Membuat entrypoint runner script (init_guest.sh)...")
        entry_script = self.rootfs / "init_guest.sh"
        script_content = """#!/system/bin/sh
# Android 15 Rootless Guest Entrypoint
export PATH=/apex/com.android.runtime/bin:/apex/com.android.art/bin:/system/bin:/system/xbin
export ANDROID_ROOT=/system
export ANDROID_DATA=/data
export ANDROID_RUNTIME_ROOT=/apex/com.android.runtime
export ANDROID_TZDATA_ROOT=/apex/com.android.tzdata
export ANDROID_ART_ROOT=/apex/com.android.art
export BOOTCLASSPATH=/apex/com.android.art/javalib/core-oj.jar:/apex/com.android.art/javalib/core-libart.jar:/apex/com.android.art/javalib/okhttp.jar:/apex/com.android.art/javalib/bouncycastle.jar:/apex/com.android.art/javalib/apache-xml.jar:/system/framework/framework.jar:/system/framework/framework-graphics.jar:/system/framework/ext.jar

echo "[GUEST] Android 15 Virtual Environment Ready."
exec /system/bin/sh
"""
        entry_script.write_text(script_content, encoding="utf-8")
        try:
            os.chmod(entry_script, 0o755)
        except Exception:
            pass
        log("Entrypoint script berhasil dibuat di /init_guest.sh")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 patch_rootfs.py <path_to_rootfs_dir>")
        sys.exit(1)

    target_dir = sys.argv[1]
    patcher = Android15RootfsPatcher(target_dir)
    patcher.run_all_patches()

if __name__ == "__main__":
    main()
