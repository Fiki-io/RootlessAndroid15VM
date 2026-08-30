package com.vm.engine;

import android.view.Surface;

/**
 * VMEngine: JNI Wrapper untuk mengontrol Rootless Android Virtual Machine.
 */
public class VMEngine {

    static {
        System.loadLibrary("vm_engine");
    }

    private static VMEngine sInstance;

    private VMEngine() {}

    public static synchronized VMEngine getInstance() {
        if (sInstance == null) {
            sInstance = new VMEngine();
        }
        return sInstance;
    }

    public boolean start(String rootfsPath, String entrypoint) {
        return nativeStartVM(rootfsPath, entrypoint);
    }

    public void setSurface(Surface surface) {
        nativeSetSurface(surface);
    }

    public void sendTouchEvent(int action, float x, float y) {
        nativeSendTouchEvent(action, x, y);
    }

    public void stop() {
        nativeStopVM();
    }

    // Native C++ Declarations
    private native boolean nativeStartVM(String rootfsPath, String entrypoint);
    private native void nativeSetSurface(Surface surface);
    private native void nativeSendTouchEvent(int action, float x, float y);
    private native void nativeStopVM();
}
