package com.vm.engine;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends Activity {

    private VMSurfaceView mSurfaceView;
    private Button mBtnStartStop;
    private ProgressBar mProgressBar;
    private TextView mTvStatus;

    private ROMManager mRomManager;
    private boolean mIsRunning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        mRomManager = new ROMManager(this);

        android.widget.LinearLayout rootLayout = new android.widget.LinearLayout(this);
        rootLayout.setOrientation(android.widget.LinearLayout.VERTICAL);
        rootLayout.setBackgroundColor(0xFF121212);

        mTvStatus = new TextView(this);
        mTvStatus.setText("Status: Memeriksa ROM Android 15...");
        mTvStatus.setTextColor(0xFFFFFFFF);
        mTvStatus.setPadding(24, 24, 24, 12);
        rootLayout.addView(mTvStatus);

        mProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mProgressBar.setVisibility(View.GONE);
        rootLayout.addView(mProgressBar);

        mBtnStartStop = new Button(this);
        mBtnStartStop.setText("Mulai Virtual Machine");
        mBtnStartStop.setBackgroundColor(0xFF00E676);
        mBtnStartStop.setTextColor(0xFF000000);
        rootLayout.addView(mBtnStartStop);

        mSurfaceView = new VMSurfaceView(this);
        android.widget.LinearLayout.LayoutParams surfaceParams = new android.widget.LinearLayout.LayoutParams(
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT,
                android.widget.LinearLayout.LayoutParams.MATCH_PARENT
        );
        surfaceParams.setMargins(0, 16, 0, 0);
        rootLayout.addView(mSurfaceView, surfaceParams);

        setContentView(rootLayout);

        setupControls();
    }

    private void setupControls() {
        if (!mRomManager.isROMInstalled()) {
            mTvStatus.setText("Status: ROM Android 15 belum terinstall.");
            mBtnStartStop.setText("Install ROM Android 15");
        } else {
            mTvStatus.setText("Status: ROM Siap Dijalankan.");
            mBtnStartStop.setText("Start Android 15");
        }

        mBtnStartStop.setOnClickListener(v -> {
            if (!mRomManager.isROMInstalled()) {
                File localArchive = new File("/sdcard/Download/android15_rootfs.tar.xz");
                if (!localArchive.exists()) {
                    Toast.makeText(this, "Letakkan file android15_rootfs.tar.xz di folder Download!", Toast.LENGTH_LONG).show();
                    return;
                }

                mProgressBar.setVisibility(View.VISIBLE);
                mTvStatus.setText("Mengekstrak ROM...");
                mRomManager.installROMFromLocalFile(localArchive, new ROMManager.ProgressCallback() {
                    @Override
                    public void onProgress(int percent, String message) {
                        runOnUiThread(() -> {
                            mProgressBar.setProgress(percent);
                            mTvStatus.setText("Progress: " + percent + "% - " + message);
                        });
                    }

                    @Override
                    public void onSuccess(File rootfsDir) {
                        runOnUiThread(() -> {
                            mProgressBar.setVisibility(View.GONE);
                            mTvStatus.setText("Status: ROM Terpasang.");
                            mBtnStartStop.setText("Start Android 15");
                            Toast.makeText(MainActivity.this, "ROM Berhasil Dipasang!", Toast.LENGTH_SHORT).show();
                        });
                    }

                    @Override
                    public void onError(String error) {
                        runOnUiThread(() -> {
                            mProgressBar.setVisibility(View.GONE);
                            mTvStatus.setText("Error: " + error);
                            Toast.makeText(MainActivity.this, "Gagal: " + error, Toast.LENGTH_LONG).show();
                        });
                    }
                });
            } else {
                if (!mIsRunning) {
                    // Mulai VM
                    File rootfs = mRomManager.getRootfsDir();
                    boolean success = VMEngine.getInstance().start(rootfs.getAbsolutePath(), "/init_guest.sh");
                    if (success) {
                        mIsRunning = true;
                        mBtnStartStop.setText("Stop Virtual Machine");
                        mBtnStartStop.setBackgroundColor(0xFFFF5252);
                        mTvStatus.setText("Status: Android 15 Sedang Berjalan (Rootless).");

                        // Mulai Foreground Service
                        Intent serviceIntent = new Intent(this, VMForegroundService.class);
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            startForegroundService(serviceIntent);
                        } else {
                            startService(serviceIntent);
                        }
                    }
                } else {
                    // Hentikan VM
                    VMEngine.getInstance().stop();
                    mIsRunning = false;
                    mBtnStartStop.setText("Start Android 15");
                    mBtnStartStop.setBackgroundColor(0xFF00E676);
                    mTvStatus.setText("Status: Virtual Machine Dihentikan.");

                    // Hentikan Foreground Service
                    Intent serviceIntent = new Intent(this, VMForegroundService.class);
                    stopService(serviceIntent);
                }
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mIsRunning) {
            VMEngine.getInstance().stop();
            Intent serviceIntent = new Intent(this, VMForegroundService.class);
            stopService(serviceIntent);
        }
    }
}
