package com.vm.engine;

import android.content.Context;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * ROMManager: Menangani download langsung dari GitHub Release, verifikasi,
 * dan ekstraksi RootFS Android 15.
 */
public class ROMManager {

    private static final String TAG = "ROMManager";
    public static final String DEFAULT_ROM_URL = 
            "https://github.com/Fiki-io/RootlessAndroid15VM/releases/download/v1.0.0-a15/android15_rootfs.tar.xz";

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

    public void downloadAndInstallROM(String downloadUrl, ProgressCallback callback) {
        new Thread(() -> {
            File tempArchive = new File(mContext.getCacheDir(), "android15_rootfs.tar.xz");
            try {
                callback.onProgress(5, "Menghubungkan ke GitHub Release...");
                URL url = new URL(downloadUrl);
                HttpURLConnection connection = (HttpURLConnection) url.openConnection();
                connection.setInstanceFollowRedirects(true);
                connection.connect();

                int fileLength = connection.getContentLength();
                InputStream input = new BufferedInputStream(connection.getInputStream());
                FileOutputStream output = new FileOutputStream(tempArchive);

                byte[] data = new byte[8192];
                long total = 0;
                int count;
                while ((count = input.read(data)) != -1) {
                    total += count;
                    if (fileLength > 0) {
                        int percent = (int) (total * 50 / fileLength); // 0% - 50% untuk download
                        callback.onProgress(5 + percent, "Mengunduh ROM (" + (total / (1024 * 1024)) + " MB)...");
                    }
                    output.write(data, 0, count);
                }

                output.flush();
                output.close();
                input.close();

                // Lanjut ke ekstraksi
                installROMFromLocalFile(tempArchive, callback);

            } catch (Exception e) {
                Log.e(TAG, "Error downloading ROM", e);
                callback.onError("Gagal mengunduh ROM: " + e.getMessage());
            }
        }).start();
    }
}
