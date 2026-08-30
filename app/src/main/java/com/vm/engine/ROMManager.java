package com.vm.engine;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * ROMManager: Menangani download, verifikasi, dan ekstraksi RootFS Android 15.
 */
public class ROMManager {

    private static final String TAG = "ROMManager";
    private final Context mContext;

    public interface ProgressCallback {
        void onProgress(int percent, String message);
        void onSuccess(File rootfsDir);
        void onError(String error);
    }

    public ROMManager(Context context) {
        mContext = context;
    }

    public File getRootfsDir() {
        return new File(mContext.getFilesDir(), "rootfs");
    }

    public boolean isROMInstalled() {
        File rootfs = getRootfsDir();
        File systemDir = new File(rootfs, "system");
        File entryScript = new File(rootfs, "init_guest.sh");
        return systemDir.exists() && entryScript.exists();
    }

    public void installROMFromLocalFile(File archiveFile, ProgressCallback callback) {
        new Thread(() -> {
            try {
                callback.onProgress(10, "Menyiapkan direktori...");
                File rootfsDir = getRootfsDir();
                if (!rootfsDir.exists()) {
                    rootfsDir.mkdirs();
                }

                callback.onProgress(30, "Mengekstrak file ROM Android 15...");
                // Jalankan ekstraksi tar / archive
                Process process = Runtime.getRuntime().exec(new String[]{
                        "tar", "-xf", archiveFile.getAbsolutePath(), "-C", rootfsDir.getAbsolutePath()
                });
                int exitCode = process.waitFor();

                if (exitCode == 0 && isROMInstalled()) {
                    callback.onProgress(100, "Instalasi selesai!");
                    callback.onSuccess(rootfsDir);
                } else {
                    callback.onError("Gagal mengekstrak arsip ROM (Exit Code: " + exitCode + ")");
                }
            } catch (Exception e) {
                Log.e(TAG, "Error installing ROM", e);
                callback.onError(e.getMessage());
            }
        }).start();
    }
}
